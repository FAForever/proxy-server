#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/signal.h>
#include <arpa/inet.h>

#include <set>
#include <vector>
#include <algorithm>

#define DEFAULT_PORT 9134

#define MAX_DESC_PER_MESSAGE 256
#define MAX_CONTROL_MESSAGE_SIZE (4 + MAX_DESC_PER_MESSAGE * sizeof(int))
#define MAX_CONTROL_MESSAGE_CONTROL_SIZE (MAX_DESC_PER_MESSAGE * CMSG_SPACE(sizeof(int)))

template <int size, bool C>
struct optional_buf {
	char value[size];
	static const bool placeholder = false;
};
template <int size>
struct optional_buf<size, false> {
	char value[1];
	static const bool placeholder = true;
};

#define MAX_CONTROL_MESSAGE_TOTAL_SIZE (MAX_CONTROL_MESSAGE_SIZE + MAX_CONTROL_MESSAGE_CONTROL_SIZE)

#define FDCTX_BUFFER_SIZE 3072

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

struct fd_ctx;

int total_connections = 0;
int total_sockets = 0;


#define MAX_PEERS 16
struct proxy_peers {
	int npeers;
	fd_ctx * peers[MAX_PEERS];
	void add(fd_ctx * p);
	fd_ctx * find(int uid);
	void remove(fd_ctx * p);
	void cleanup_dangling();
	void remove_from_all_peers(fd_ctx * p);
	void unref_all();
	proxy_peers() : npeers(0) { }
};

struct fd_ctx {
	int faf_uid;
	int fd;
	int buf_len;
	int refcount;
	int protocol;
	bool is_server;
	proxy_peers peers;
	char buf[FDCTX_BUFFER_SIZE];

	void remove_myself_from_peer_caches() {
		peers.remove_from_all_peers(this);
	}
	void cache_remove(fd_ctx * p) {
		peers.remove(p);
	}
	fd_ctx() : refcount(1) { ++total_connections; }
	~fd_ctx();
};

fd_ctx * proxy_peers::find(int uid) {
	for (int i = 0; i < npeers; ++i) {
		if (peers[i]->faf_uid == uid) return peers[i];
	}
	return NULL;
}

void proxy_peers::add(fd_ctx * p) {
	if (npeers < MAX_PEERS) {
		// insert fresh entries at the front
		fd_ctx * prev = peers[0];
		for (int i = 1; i < npeers; ++i) {
			fd_ctx * tmp = peers[i];
			peers[i] = prev;
			prev = tmp;
		}
		peers[npeers] = prev;
		peers[0] = p;
		++p->refcount;
		++npeers;
	} else {
		// remove oldest mapping
		if (--peers[npeers - 1]->refcount == 0) {
			delete peers[npeers - 1];
		}
		--npeers;
		add(p);
	}
}

void proxy_peers::remove(fd_ctx * p) {
	for (int i = 0; i < npeers; ++i) {
		if (peers[i] == p) {
			++i;
			for (; i < npeers; ++i) {
				peers[i - 1] = peers[i];
			}
			--npeers;
			if (--p->refcount == 0) {
				delete p;
			}
			return;
		}
	}
}

void proxy_peers::remove_from_all_peers(fd_ctx * p) {
	for (int i = 0; i < npeers; ++i) {
		peers[i]->cache_remove(p);
	}
}

void proxy_peers::unref_all() {
	for (int i = 0; i < npeers; ++i) {
		if (--peers[i]->refcount == 0) {
			delete peers[i];
		}
	}
	npeers = 0;
}

void proxy_peers::cleanup_dangling() {
	int oi = 0;
	for (int i = 0; i < npeers; ++i) {
		if (peers[i]->faf_uid == -1) {
			if (--peers[i]->refcount == 0) {
				delete peers[i];
				continue;
			}
		}
		if (i != oi) peers[oi] = peers[i];
		++oi;
	}
	npeers = oi;
}

struct fd_ctx_less_by_uid {
	bool operator()(const fd_ctx * a, const fd_ctx * b) const {
		return a->faf_uid < b->faf_uid;
	}
};

struct proxy_msg_header {
	uint32_t size;
	uint16_t port;
	uint16_t destuid;
} __attribute__ ((packed));

struct proxy_msg_header_set_uid {
	uint32_t size;
	uint16_t uid;
} __attribute__ ((packed));

struct proxy_msg_header_to_peer {
	uint32_t size;
	uint16_t port;
} __attribute__ ((packed));

typedef std::set<fd_ctx *, fd_ctx_less_by_uid> peer_sockets_t;

#define PEER_CTX_BUF_SIZE 3072
#define OUT_HEADER_OFFSET_ADJ (sizeof(proxy_msg_header) - sizeof(proxy_msg_header_to_peer))

bool got_sigusr1 = false;

void sigusr1(int) {
	got_sigusr1 = true;
}

fd_ctx::~fd_ctx() {
	peers.unref_all();
	--total_connections;
}

char * get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen) {
    switch(sa->sa_family) {
	case AF_INET:
		inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
				  s, maxlen);
		break;
		
	case AF_INET6:
		inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
				  s, maxlen);
		break;
		
	default:
		strncpy(s, "(unknown)", maxlen);
		return NULL;
    }
	
    return s;
}

int ctrl_socket_listen(int s, const char * path) {
	sockaddr_un sun;
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, path, sizeof(sun.sun_path));

	if (bind(s, (sockaddr *) &sun, sizeof(sun)) < 0) {
		fprintf(stderr, "bind(%s): %s\n", path, strerror(errno));
		return -1;
	}
	int on = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) == -1) {
		perror("setsockopt(REUSEADDR)");
		return -1;
	}
	if (listen(s, 1) < 0) {
		perror("listen"); return -1;
	}
	return 0;
}

int poll_in(int epoll, fd_ctx * ptr) {
	epoll_event ev;
	ev.events   = EPOLLIN;
	ev.data.ptr = (void *) ptr;
	if (epoll_ctl(epoll, EPOLL_CTL_ADD, ptr->fd, &ev) < 0) {
		perror("epoll_ctl"); return -1;
	}
	return 0;
}

template <typename Iter, typename Container>
int send_fds(int ctrlsock, Iter beg, Iter end, Container * all) {
	char control[CMSG_SPACE(sizeof(int) * MAX_DESC_PER_MESSAGE)];
	char buf[4 + sizeof(int) * MAX_DESC_PER_MESSAGE];
	msghdr msg;

	msg.msg_name       = NULL;
	msg.msg_namelen    = 0;
	msg.msg_control    = control;
	msg.msg_controllen = sizeof(control);

	strcpy(buf, "desc");

	cmsghdr * cmp = CMSG_FIRSTHDR(&msg);

	cmp->cmsg_level = SOL_SOCKET;
	cmp->cmsg_type  = SCM_RIGHTS;

	int fd_count = 0;
	Iter erase_beg;
	bool erase_valid = false;
	std::vector<int> to_close;

	for (int * uidp = (int *) (buf + 4);
		 beg != end && fd_count < MAX_DESC_PER_MESSAGE;
		 ++beg, ++uidp)
	    {
			if ((**beg).buf_len == 0) {
				if (! erase_valid) {
					erase_beg = beg;
				    erase_valid = true;
				}
				*uidp = (**beg).faf_uid;

				* ((int *) CMSG_DATA(cmp) + fd_count) = (**beg).fd;
				to_close.push_back((**beg).fd);
				//if (epoll_ctl(epoll, EPOLL_CTL_DEL, (**beg).fd, NULL) < 0) {
				//					perror("epoll_ctl(DEL)");
				//				}
				++fd_count;
			} else {
				if (erase_valid) {
					if (all) all->erase(erase_beg, beg);
					erase_valid = false;
				}
			}
		}

	cmp->cmsg_len   = CMSG_LEN(sizeof(int) * fd_count);
	if (erase_valid) {
		if (all) all->erase(erase_beg, beg);
	}
	msg.msg_controllen  = CMSG_SPACE(fd_count * sizeof(int));
	iovec iov;
	iov.iov_base   = buf;
	iov.iov_len    = 4 + fd_count * sizeof(int);
	msg.msg_iov    = &iov;
	msg.msg_iovlen = 1;

	if (fd_count) {
		if (sendmsg(ctrlsock, &msg, 0) < 0) {
			perror("sendmsg");
		} else {
			total_sockets -= to_close.size();
			total_connections -= to_close.size();
			for (int i = 0; i < to_close.size(); ++i) {
				close(to_close[i]);
			}
		}
	}
	return fd_count;
}

template <typename T>
struct dummy_erase_container {
	void erase(T *, T *) { }
};

int send_fd(int ctrlsock, fd_ctx * ctxp) {
	return send_fds(ctrlsock, &ctxp, &ctxp + 1, (dummy_erase_container<fd_ctx *> *) NULL);
}

int main(int argc, char ** argv) {
	int listen_port = -1;
	char listen_port_str[8];
	const char * ctrl_socket_path = NULL;

	{
		int opt;
		while ((opt = getopt(argc, argv, "p:hu:")) != EOF) {
			switch (opt) {
			case 'p' :
				listen_port = atoi(optarg);
				break;
			case 'h' :
				fprintf(stderr, "%s [-p port] [-u socket-path]\n", argv[0]);
				fprintf(stderr, "default: -p 9134\n");
				exit(0);
			case 'u' :
				ctrl_socket_path = optarg;
				break;
			}
		}
		argc -= optind;
		argv += optind;
	}

	if (listen_port == -1) {
		listen_port = 9134;
	}
	sprintf(listen_port_str, "%d", listen_port);

	typedef std::vector<fd_ctx> server_sockets_t;
	server_sockets_t server_sockets;
	peer_sockets_t peer_sockets;

	fd_ctx ctrl_socket, ctrl_socket_conn;
	bool ctrl_socket_mode_listen = false;
	bool decay_mode = false;

	ctrl_socket.fd = -1;
	ctrl_socket_conn.fd = -1;

	int sockets_inherited = 0;

	int epoll = epoll_create(1024);
	if (epoll < 0) {
		perror("epoll_create"); exit(1);
	}

	if (ctrl_socket_path) {
		int s = socket(PF_UNIX, SOCK_SEQPACKET, 0);
		if (s < 0) {
			perror("socket(AF_UNIX)");
			exit(1);
		}
		struct sockaddr_un sun;
		sun.sun_family = AF_UNIX;
		strncpy(sun.sun_path, ctrl_socket_path, sizeof(sun.sun_path));

		if (connect(s, (sockaddr *) &sun, sizeof(sun))) {
			if (errno == ECONNREFUSED || errno == ENOENT) {
				if (errno == ECONNREFUSED) {
					if (unlink(ctrl_socket_path) < 0) {
						fprintf(stderr, "unlink(%s): %s\n", ctrl_socket_path, strerror(errno));
						exit(1);
					}
				}
				ctrl_socket_listen(s, ctrl_socket_path);
				ctrl_socket.fd = s;
				poll_in(epoll, &ctrl_socket);
				ctrl_socket_mode_listen = true;
			} else {
				fprintf(stderr, "connect(%s): %s\n", ctrl_socket_path, strerror(errno));
			}
		} else {
			char buf[16];
			strcpy(buf, "unlisten");
			ssize_t n = send(s, "unlisten", strlen("unlisten"), 0);
			if (n < 0) {
				perror("sendmsg");
				exit(1);
			}

			n = recv(s, buf, sizeof(buf), 0);
			if (strncmp(buf, "unlistening", strlen("unlistening")) != 0) {
				fprintf(stderr, "running server reported: ");
				fwrite(buf, n, 1, stderr);
				exit(1);
			}
			ctrl_socket_conn.fd = s;
			poll_in(epoll, &ctrl_socket_conn);
		}
	}

	{
		struct addrinfo hints, * ai_res;
		hints.ai_family   = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags    = AI_PASSIVE;

		int r = getaddrinfo(NULL, listen_port_str, &hints, &ai_res);
		if (r) {
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
			exit(1);
		}

		for (struct addrinfo * ai = ai_res; ai; ai = ai->ai_next) {
			int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
			if (s < 0) {
				perror("socket"); exit(1);
			}
			if (ai->ai_family == AF_INET6) {
				int on = 1;
				if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, 
							   (char *)&on, sizeof(on)) == -1) {
					perror("setsockopt(IPV6_ONLY)");
					exit(1);
				}
			}
			{
				int on = 1;
				if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on)) == -1) {
					perror("setsockopt(REUSEADDR)");
					exit(1);
				}
			}
			if (bind(s, ai->ai_addr, ai->ai_addrlen) < 0) {
				perror("bind"); exit(1);
			}
			if (listen(s, 5) < 0) {
				perror("listen"); exit(1);
			}
			fd_ctx c;
			c.fd = s;
			c.is_server = true;
			c.protocol = ai->ai_protocol;
			char * strp = c.buf;
			int slen  = sizeof(c.buf);
			if (ai->ai_family == AF_INET6) {
				*strp++ = '[';
				slen -= 2;
			}
			get_ip_str(ai->ai_addr, strp, slen);
			if (ai->ai_family == AF_INET6) {
				strcat(c.buf, "]");
			}
			sprintf(c.buf + strlen(c.buf), ":%d", listen_port);
			server_sockets.push_back(c);
		}
		freeaddrinfo(ai_res);
	}

	for (int i = 0; i < server_sockets.size(); ++i) {
		poll_in(epoll, &server_sockets[i]);
	}

	epoll_event epoll_events[32];
	const int epoll_max_events = 32;

	fd_ctx fd_ctx_finder;
	signal(SIGUSR1, sigusr1);
	signal(SIGPIPE, SIG_IGN);

	total_sockets  = server_sockets.size();
	time_t status_time = time(NULL);

	while (total_sockets) {
		if (unlikely(got_sigusr1)) {
			// close listening sockets
			for (int i = 0; i < server_sockets.size(); ++i) {
				fprintf(stderr, "close server %s\n", server_sockets[i].buf);
				if (epoll_ctl(epoll, EPOLL_CTL_DEL, server_sockets[i].fd, NULL) < 0) {
					perror("epoll_ctl");
				}
				close(server_sockets[i].fd);
				--total_sockets;
			}
			got_sigusr1 = false;
		}
		if (unlikely(status_time + 5 < time(NULL))) {
			fprintf(stderr, "%d connections, %d identified peers\n", total_connections - server_sockets.size(), peer_sockets.size());
			status_time = time(NULL);
		}

		int ep_num = epoll_wait(epoll, epoll_events, epoll_max_events, 1000);
		if (unlikely(ep_num < 0)) {
			if (errno == EINTR) continue;
			perror("epoll_wait"); continue;
		}
		bool epoll_restart = false;
		for (int epi = 0; epi < ep_num && ! epoll_restart; ++epi) {
			fd_ctx * ctxp = (fd_ctx *) epoll_events[epi].data.ptr;

			if (unlikely(ctxp == &ctrl_socket)) {
				sockaddr_storage ss;
				socklen_t sl = sizeof(ss);
				
				int nsock = accept(ctxp->fd, (sockaddr *) &ss, &sl);
				if (nsock < 0) {
					perror("accept"); exit(1);
				}
				ctrl_socket_conn.fd = nsock;
				epoll_event ev;
				ev.events   = EPOLLIN;
				ev.data.ptr = (void *) &ctrl_socket_conn;
				if (epoll_ctl(epoll, EPOLL_CTL_ADD, ctrl_socket_conn.fd, &ev) < 0) {
					perror("epoll_ctl"); exit(1);
				}
				if (epoll_ctl(epoll, EPOLL_CTL_DEL, ctrl_socket.fd, NULL) < 0) {
					perror("epoll_ctl"); exit(1);
				}
			} else if (unlikely(ctxp == &ctrl_socket_conn)) {
				if (ctrl_socket_mode_listen) {
					char buf[1024];

					int n = read(ctxp->fd, buf, sizeof(buf));
					if (n < 0) {
						perror("read");
					} else {
						if (strncmp(buf, "unlisten", strlen("unlisten")) == 0) {
							for (int i = 0; i < server_sockets.size(); ++i) {
								fprintf(stderr, "close server %s\n", server_sockets[i].buf);
								if (epoll_ctl(epoll, EPOLL_CTL_DEL, server_sockets[i].fd, NULL) < 0) {
									perror("epoll_ctl");
								}
								close(server_sockets[i].fd);
								--total_sockets;
							}
							if (write(ctrl_socket_conn.fd, "unlistening", strlen("unlistening")) < 0) {
								perror("write");
							}
							int nsent = 0;
							do {
								nsent = send_fds(ctrl_socket_conn.fd, peer_sockets.begin(), peer_sockets.end(), &peer_sockets);
								fprintf(stderr, "send at once: %d\n", nsent);
							} while (nsent && ! peer_sockets.empty());
							epoll_restart = true;
							decay_mode = true;
						}
					}
				} else {
					msghdr msg;
					iovec iov;
					optional_buf<MAX_CONTROL_MESSAGE_CONTROL_SIZE, (MAX_CONTROL_MESSAGE_TOTAL_SIZE > FDCTX_BUFFER_SIZE)> control;
					char * controlp = control.placeholder ?
						ctxp->buf + MAX_CONTROL_MESSAGE_SIZE :
						control.value;
					optional_buf<MAX_CONTROL_MESSAGE_SIZE, (MAX_CONTROL_MESSAGE_SIZE > FDCTX_BUFFER_SIZE)> buf;
					char * bufp = buf.placeholder ?	ctxp->buf : control.value;

					iov.iov_base = bufp;
					iov.iov_len  = MAX_CONTROL_MESSAGE_SIZE;

					msg.msg_name       = NULL;
					msg.msg_namelen    = 0;
					msg.msg_iov        = &iov;
					msg.msg_iovlen     = 1;
					msg.msg_control    = (void *) controlp;
					msg.msg_controllen = MAX_CONTROL_MESSAGE_CONTROL_SIZE;
					msg.msg_flags      = 0;

					int n = recvmsg(ctxp->fd, &msg, 0);
					if (n < 0) {
						perror("recvmsg");
					} else if (n == 0) {
						fprintf(stderr, "unexpected close\n");
						close(ctxp->fd);
					} else {
						if (strncmp((const char *) iov.iov_base, "desc", std::min(4, (int) iov.iov_len)) == 0) {
							cmsghdr * cmp = CMSG_FIRSTHDR(&msg);
							if (cmp->cmsg_level != SOL_SOCKET || cmp->cmsg_type != SCM_RIGHTS) {
								fprintf(stderr, "malformed control message: wrong type\n");
								exit(1);
							}

							int * uidp = (int *) ((char *) iov.iov_base + 4);
							int * uidpend = (int *) ((char *) iov.iov_base + n);

							int fd_count = 0;
							for (; uidp < uidpend; ++uidp, ++fd_count) {
								int fd = * ((int *) CMSG_DATA(cmp) + fd_count);
								++sockets_inherited;
								++total_sockets;
								fd_ctx * cp = new fd_ctx;
								cp->fd = fd;
								cp->faf_uid = *uidp;
								cp->is_server = false;
								cp->protocol = IPPROTO_TCP;
								cp->buf_len = 0;
								epoll_event ev;
								ev.events = EPOLLIN;
								ev.data.ptr = (void *) cp;
								if (epoll_ctl(epoll, EPOLL_CTL_ADD, fd, &ev) < 0) {
									perror("epoll_ctl");
									--total_sockets;
									close(fd);
									delete cp;
								}
								if (cp->faf_uid != -1) {
									peer_sockets.insert(cp);
								}
							}
						} else if (strncmp((const char *) iov.iov_base, "exit", 4) == 0) {
							close(ctxp->fd);
							int s = socket(PF_UNIX, SOCK_SEQPACKET, 0);
							if (s < 0) {
								perror("socket(PF_UNIX)");
							} else {
								ctrl_socket_listen(s, ctrl_socket_path);
								ctrl_socket.fd = s;
								poll_in(epoll, &ctrl_socket);
								ctrl_socket_mode_listen = true;
							}
							fprintf(stderr, "%d sockets inherited from the dead\n", sockets_inherited);
						}
					}
				}
			} else if (unlikely(ctxp->is_server && ctxp->protocol == IPPROTO_TCP)) {
				sockaddr_storage saddr;
				socklen_t saddrlen = sizeof(saddr);
				int nsock = accept(ctxp->fd, (sockaddr *) &saddr, &saddrlen);
				if (nsock < 0) {
					perror("accept");
				} else {
					++total_sockets;
					fd_ctx * cp = new fd_ctx;
					cp->fd = nsock;
					cp->faf_uid = -1;
					cp->is_server = false;
					cp->protocol = IPPROTO_TCP;
					cp->buf_len = 0;
					epoll_event ev;
					ev.events = EPOLLIN;
					ev.data.ptr = (void *) cp;
					if (epoll_ctl(epoll, EPOLL_CTL_ADD, nsock, &ev) < 0) {
						perror("epoll_ctl");
						--total_sockets;
						close(nsock);
						delete cp;
					}
				}
			} else {
				if (unlikely(decay_mode) && ctxp->buf_len == 0) {
					send_fd(ctrl_socket_conn.fd, ctxp);
				}

				int n = read(ctxp->fd, ctxp->buf + ctxp->buf_len, PEER_CTX_BUF_SIZE - ctxp->buf_len);
				if (unlikely(n < 0)) {
					if (errno != ECONNRESET && errno != EAGAIN && errno != EINTR) {
						perror("read");
					}
					continue;
				} else if (unlikely(n == 0)) {
					close(ctxp->fd);
					--total_sockets;
					if (ctxp->faf_uid != -1) {
						peer_sockets.erase(ctxp);
					}
					ctxp->remove_myself_from_peer_caches();
					--ctxp->refcount;
					if (ctxp->refcount == 0) {
						delete ctxp;
					} else {
						ctxp->faf_uid = -1;
					}
				} else {
					ctxp->buf_len += n;
					char * buf_head = ctxp->buf;
					bool postprocess = true;

					while (buf_head < ctxp->buf + ctxp->buf_len) {
						proxy_msg_header * h = (proxy_msg_header *) buf_head;
						const int buf_len = ctxp->buf + ctxp->buf_len - buf_head;
						const int in_msg_size = ntohl(h->size);

						if (buf_len < 4) {
							break;
						}
						
						if (unlikely(buf_len > PEER_CTX_BUF_SIZE)) {
							// message to big
							if (epoll_ctl(epoll, EPOLL_CTL_DEL, ctxp->fd, NULL) < 0) {
								perror("epoll_ctl");
							}
							close(ctxp->fd);
							--total_sockets;
							if (ctxp->faf_uid != -1) {
								peer_sockets.erase(ctxp);
							}
							ctxp->remove_myself_from_peer_caches();
							--ctxp->refcount;
							if (ctxp->refcount == 0) {
								delete ctxp;
							} else {
								ctxp->faf_uid = -1;
							}
							postprocess = false;
							break;
						}

						if (in_msg_size + 4 > buf_len) {
							break;
						}

						if (unlikely(ctxp->faf_uid == -1)) {
							proxy_msg_header_set_uid * hu = (proxy_msg_header_set_uid *) h;
							ctxp->faf_uid = ntohs(hu->uid);
							peer_sockets.insert(ctxp);

							buf_head += in_msg_size + 4;
							continue;
						}
						int uid = ntohs(h->destuid);

						fd_ctx * peer = ctxp->peers.find(uid);

						if (unlikely(! peer)) {
							fd_ctx_finder.faf_uid = uid;
							peer_sockets_t::iterator iter = peer_sockets.find(&fd_ctx_finder);
							if (iter != peer_sockets.end()) {
								peer = *iter;
								ctxp->peers.add(peer);
							} else {
								buf_head += in_msg_size + 4;
								continue;
							}
						}

						int in_port = ntohs(h->port);
						proxy_msg_header_to_peer * hout = (proxy_msg_header_to_peer *) (buf_head + OUT_HEADER_OFFSET_ADJ);
						hout->port = htons(in_port);
						const int out_size = in_msg_size - OUT_HEADER_OFFSET_ADJ;
						hout->size = htonl(out_size);

						{
							int n = write(peer->fd, (char *) hout, out_size + 4);
							if (unlikely(n < 0)) {
								if (errno != ECONNRESET) {
									perror("write");
								}
							} else if (unlikely(n != out_size + 4)) {
								fprintf(stderr, "short write (%d of %d\n", n, out_size + 4);
							}
						}
						buf_head += in_msg_size + 4;
					}
					if (likely(postprocess)) {
						int new_buflen = ctxp->buf + ctxp->buf_len - buf_head;

						if (unlikely(new_buflen && ctxp->buf != buf_head)) {
							for (char * p = ctxp->buf; buf_head < ctxp->buf + ctxp->buf_len; ++p, ++buf_head) {
								*p = *buf_head;
							}
						}
						ctxp->buf_len = new_buflen;
					}
				}
			}
		}
	}
	if (decay_mode && ctrl_socket_path) {
		close(ctrl_socket.fd);
		unlink(ctrl_socket_path);
		if (write(ctrl_socket_conn.fd, "exit", strlen("exit")) < 0) {
			perror("send");
		}
	}
	fprintf(stderr, "exit due to %d sockets left to serve\n", total_sockets);
	exit(0);
}
