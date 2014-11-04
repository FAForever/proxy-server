#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include <sys/epoll.h>
#include <sys/signal.h>
#include <arpa/inet.h>

#include <set>
#include <vector>

#define likely(x) __builtin_expect((x),1)
#define unlikely(x) __builtin_expect((x),0)

struct fd_ctx;

int total_connections = 0;

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
	char buf[3072];

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
		peers[npeers] = p;
		++p->refcount;
		++npeers;
	} else {
		// remove oldest mapping
		if (--peers[0]->refcount == 0) {
			delete peers[0];
		}
		peers[0] = p;
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

int main(int argc, char ** argv) {
	int listen_port = -1;
	char listen_port_str[8];

	{
		int opt;
		while ((opt = getopt(argc, argv, "p:")) != EOF) {
			switch (opt) {
			case 'p' :
				listen_port = atoi(optarg);
				break;
			case 'h' :
				fprintf(stderr, "%s [-p port]\n", argv[0]);
				fprintf(stderr, "default: -p 9124\n");
				exit(0);
			}
		}
		argc -= optind;
		argv += optind;
	}

	if (listen_port == -1) {
		listen_port = 9124;
	}
	sprintf(listen_port_str, "%d", listen_port);

	typedef std::vector<fd_ctx> server_sockets_t;
	server_sockets_t server_sockets;
	typedef std::set<fd_ctx *, fd_ctx_less_by_uid> peer_sockets_t;
	peer_sockets_t peer_sockets;

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
			get_ip_str(ai->ai_addr, c.buf, sizeof(c.buf));
			server_sockets.push_back(c);
		}
		freeaddrinfo(ai_res);
	}

	int epoll = epoll_create(1024);
	if (epoll < 0) {
		perror("epoll_create"); exit(1);
	}

	{
		for (int i = 0; i < server_sockets.size(); ++i) {
			epoll_event ev;
			ev.events = EPOLLIN;
			ev.data.ptr = (void *) &(server_sockets[i]);
			if (epoll_ctl(epoll, EPOLL_CTL_ADD, server_sockets[i].fd, &ev) < 0) {
				perror("epoll_ctl"); exit(1);
			}
		}
	}

	epoll_event epoll_events[32];
	const int epoll_max_events = 32;

	fd_ctx fd_ctx_finder;
	signal(SIGUSR1, sigusr1);
	signal(SIGPIPE, SIG_IGN);

	int total_sockets  = server_sockets.size();
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
			fprintf(stderr, "%d connections, %d identified peers\n", total_connections, peer_sockets.size());
			status_time = time(NULL);
		}

		int ep_num = epoll_wait(epoll, epoll_events, epoll_max_events, 1000);
		if (unlikely(ep_num < 0)) {
			if (errno == EINTR) continue;
			perror("epoll_wait"); continue;
		}
		for (int epi = 0; epi < ep_num; ++epi) {
			epoll_event * ev = epoll_events + epi;
			fd_ctx * ctxp = (fd_ctx *) ev->data.ptr;
			if (unlikely(ctxp->is_server && ctxp->protocol == IPPROTO_TCP)) {
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
				int n = read(ctxp->fd, ctxp->buf + ctxp->buf_len, PEER_CTX_BUF_SIZE - ctxp->buf_len);
				if (unlikely(n < 0)) {
					if (errno != ECONNRESET && errno != EAGAIN && errno != EINTR) {
						perror("read");
					}
					continue;
				} else if (unlikely(n == 0)) {
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
	fprintf(stderr, "exit due to %d sockets left to serve\n", total_sockets);
	exit(0);
}
