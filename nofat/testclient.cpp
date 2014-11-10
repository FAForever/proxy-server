#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <string.h>

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


int main(int argc, char ** argv) {
	int uid = 3;

	for (int i = 0; i < atoi(argv[1]); ++i) {
		if (fork() == 0) {
			int sock = socket(PF_INET, SOCK_STREAM, 0);
			if (sock < 0) {
				perror("socket");
				exit(1);
			}
			struct sockaddr_in sin;
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = inet_addr("127.0.0.1");
			sin.sin_port = htons(atoi(argv[2]));
			if (connect(sock, (sockaddr *) &sin, sizeof(sin))) {
				perror("connect");
				exit(1);
			}
			
			char buf[1024];
			proxy_msg_header_set_uid * h_su = (proxy_msg_header_set_uid *) buf;
			proxy_msg_header_to_peer * h_to = (proxy_msg_header_to_peer *) buf;
			proxy_msg_header * h = (proxy_msg_header *) buf;
			int peer_uid = i & 1 ? uid - 1 : uid + 1;

			h_su->uid = htons(uid);
			h_su->size = htonl(2);
			write(sock, buf, 6);

			if (fork() == 0) {
				sleep(1);
				for (int i = 0; i < atoi(argv[3]); ++i) {
					int msgsize = rand() & (63 & ~3);
					if (msgsize < 4) msgsize = 4;
					uint32_t * p = (uint32_t *) (buf + sizeof(*h) + 9);
					p[0] = i;
					for (int j = 1; j < msgsize / 4; ++j) {
						p[j] = peer_uid + j;
					}
					memcpy(buf + sizeof(*h), "\0\0\0\x0a\0\0\0", 7);
					uint16_t * ppp = (uint16_t *) (buf + sizeof(*h) + 7);
					*ppp = htons(msgsize);
					h->destuid = htons(peer_uid);
					h->port = htons(i + 10 ==  atoi(argv[3]) ? 1 : 0);
					h->size = htonl(msgsize + sizeof(*h) + 9 - 4);
					//					fprintf(stderr, "%d %d write %d %d %d\n", getpid(), uid, peer_uid, ntohs(h->port), msgsize);
					if (write(sock, buf, sizeof(*h) + msgsize + 9) <= 0) {
						perror("write");
					}
					usleep(1000);
				}
				exit(0);
			} else {
				int nread = 0;
				char * ack = new char[atoi(argv[3])];
				memset(ack, 0, atoi(argv[3]));

				while (true) {
					int n = read(sock, buf, 4);
					uint32_t * pp = (uint32_t *) buf;
					n = read(sock, buf + 4, ntohl(*pp));

					if (n < 0) {
						perror("read");
					}
					if (n == 0) {
						close(sock);
						break;
					}

					nread++;
					uint32_t * p = (uint32_t *) (buf + sizeof(*h_to) + 9);
					int msgsize = ntohl(h_to->size) - 2 - 9;

					ack[p[0]] = 1;
					for (int j = 1; j < msgsize / 4; ++j) {
						if (p[j] != uid + j) {
							fprintf(stderr, "data error\n");
						}
					}
					//					fprintf(stderr, "%d %d read %d\n", getpid(), uid, ntohs(h_to->port));
					if (ntohs(h_to->port)) break;
				}
				int missing = 0;
				for (int j = 0; j < atoi(argv[3]) - 10; ++j) {
					if (!ack[j]) {
						fprintf(stderr, "%d ", j);
						++missing;
					}
				}
				if (missing) {
					fprintf(stderr, "quit %d missing %d received\n", missing, nread);
				}
				wait(NULL);
			}
			exit(0);
		}
		usleep(100000);
		++uid;
	}

	while (wait(NULL) != -1 || errno != ECHILD) { }
}
