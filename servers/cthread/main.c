#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <errno.h>

#include <sys/wait.h>

static int done = 0;
static const int ONE = 1;

void * handle_client(void * arg);
void sig_handler(int signo) {
	done = 1;
}

int main(int argc, char *argv[])
{
	int opt, ret;
	int port;
	long i;

	signal(SIGINT, sig_handler);
	signal(SIGKILL, sig_handler);
	signal(SIGTERM, sig_handler);

	struct sockaddr_in servaddr;
	struct timeval timeout;
	int ssock;

	long nthreads;
	pthread_t *threads;

	while ((opt = getopt(argc, argv, "p:")) != -1) {
		switch (opt) {
			case 'p':
				port = atoi(optarg);
				break;
			default: /* '?' */
				fprintf(stderr, "Usage: %s -p port\n", argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Unexpected additional arguments:");
		for (i = optind; i < argc; i++) {
			fprintf(stderr, " %s", argv[i]);
		}
		fprintf(stderr, "\n");
		exit(EXIT_FAILURE);
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	servaddr.sin_port = htons(port);

	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if (ssock == -1) {
		// TODO: handle error
	}

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	setsockopt(ssock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	ret = bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret == -1) {
		// TODO: handle error
	}

	ret = listen(ssock, 0);
	if (ret == -1) {
		// TODO: handle error
	}

	nthreads = 200;
	threads = calloc(nthreads, sizeof(pthread_t));

	for (i = 0; i < nthreads; i++) {
		ret = pthread_create(&threads[i], NULL, handle_client, &ssock);
		if (ret != 0) {
			// TODO: handle error
		}
	}
	for (i = 0; i < nthreads; i++) {
		ret = pthread_join(threads[i], NULL);
		if (ret != 0) {
			// TODO: handle error
		}
	}

	close(ssock);
	return EXIT_SUCCESS;
}

void * handle_client(void * arg) {
	int ret;
	int csock;
	int ssock = *((int *)arg);
	struct sockaddr_in client;
	socklen_t socksize = sizeof(struct sockaddr_in);

	uint32_t challenge;

	while (done == 0) {
		csock = accept(ssock, (struct sockaddr *)&client, &socksize);
		if (csock == -1) {
			if (errno == EWOULDBLOCK) {
				continue;
			}
			// TODO: handle error
		}

		setsockopt(csock, IPPROTO_TCP, TCP_NODELAY, &ONE, sizeof(ONE));

		while (done == 0) {
			ret = recvfrom(csock, &challenge , sizeof(challenge), MSG_WAITALL, NULL, NULL);
			if (ret == -1) {
				// TODO: handle error
				break;
			}
			if (ret == 0) {
				// connection closed
				break;
			}

			challenge = ntohl(challenge);
			if (challenge == 0) {
				done = 1;
				break;
			}
			challenge = htonl(challenge + 1);

			ret = sendto(csock, &challenge, sizeof(challenge), 0, NULL, 0);
			if (ret == -1) {
				// TODO: handle error
				break;
			}
		}

		close(csock);
	}

	return NULL;
}
