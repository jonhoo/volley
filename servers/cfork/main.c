#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <sys/wait.h>

int main(int argc, char *argv[])
{
	int opt, i, ret;
	int port;

	struct sockaddr_in servaddr, client;
	socklen_t socksize;
	int ssock;

	pid_t pid;
	int forks = 0;

	uint32_t challenge;

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
	socksize = sizeof(struct sockaddr_in);

	ssock = socket(AF_INET, SOCK_STREAM, 0);
	if (ssock == -1) {
		// TODO: handle error
	}

	ret = bind(ssock, (struct sockaddr *)&servaddr, sizeof(servaddr));
	if (ret == -1) {
		// TODO: handle error
	}

	ret = listen(ssock, 0);
	if (ret == -1) {
		// TODO: handle error
	}

	for (;;) {
		int csock = accept(ssock, (struct sockaddr *)&client, &socksize);
		if (csock == -1) {
			// TODO: handle error
		}

		forks++;
		if ((pid = fork()) == 0) {
			for (;;) {
				ret = recvfrom(csock, &challenge , sizeof(challenge), MSG_WAITALL, NULL, NULL);
				if (ret == -1) {
					// TODO: handle error
				}
				if (ret == 0) {
					// connection closed
					break;
				}

				challenge = htonl(ntohl(challenge) + 1);

				ret = sendto(csock, &challenge, sizeof(challenge), 0, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
				if (ret == -1) {
					// TODO: handle error
				}

			}
			close(csock);
			exit(EXIT_SUCCESS);
		}
	}

	for (i = 0; i < forks; i++) {
		wait(NULL);
	}

	close(ssock);
	return EXIT_SUCCESS;
}
