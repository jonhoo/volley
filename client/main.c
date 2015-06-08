#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <bsd/stdlib.h>
#include <pthread.h>
#include <math.h>
#include <time.h>

struct client_details {
	struct sockaddr_in * servaddr;
	long iterations;
};

struct client_stats {
	double mean;
	double S; /* stddev = sqrt(S/n) */
	long n;
};

void * client(void * arg);
const double Z = 2.576; // 99% probability estimated value
const double E = 5000;  // lies within +/- 5us of true value
const int MAX_ITERATIONS_PER_ROUND = 10000000;

int main(int argc, char** argv) {
	int port, clients = 0;
	int opt, i, ret;
	long max_iterations;
	int sockfd;

	struct sockaddr_in servaddr;
	struct client_details carg;
	pthread_t * threads;
	uint32_t zero = 0;

	struct client_stats *cret;
	double current_mean, mean = 0;
	double stddev = 0;
	long n = 0;

	while ((opt = getopt(argc, argv, "p:c:")) != -1) {
		switch (opt) {
			case 'p':
				port = atoi(optarg);
				break;
			case 'c':
				clients = atoi(optarg);
				break;
			default: /* '?' */
				fprintf(stderr, "Usage: %s -p port -c clients\n", argv[0]);
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

	carg.servaddr = &servaddr;

	threads = calloc(clients, sizeof(pthread_t));

	max_iterations = MAX_ITERATIONS_PER_ROUND / clients;
	carg.iterations = max_iterations;
	fprintf(stderr, "priming with %ld iterations across %d clients\n", carg.iterations, clients);

	for (; carg.iterations > 10;) {
		for (i = 0; i < clients; i++) {
			ret = pthread_create(&threads[i], NULL, client, &carg);
			if (ret != 0) {
				// TODO: handle error
			}
		}
		for (i = 0; i < clients; i++) {
			ret = pthread_join(threads[i], (void**) &cret);
			if (ret != 0) {
				// TODO: handle error
			}
			if (cret == PTHREAD_CANCELED) {
				// TODO: handle error
			}

			current_mean = mean;
			mean = (n * current_mean + cret->n * cret->mean) / (n + cret->n);
			stddev = sqrt((pow(stddev, 2)*n+cret->S)/(n+cret->n));
			n += cret->n;
			free(cret);
		}

		carg.iterations = (long) ceil((pow((Z * stddev) / E, 2) - n) / clients);
		fprintf(stderr, "iteration complete: mean is %.0fus, stddev is %.2fus\n", mean/1000.0, stddev/1000.0);
		if (carg.iterations > max_iterations) {
			fprintf(stderr, "need many more samples (%ld) to achieve statistical significance, doing another %ld per client\n", carg.iterations * clients, max_iterations);
			carg.iterations = max_iterations;
		} else if (carg.iterations > 0) {
			fprintf(stderr, "running %ld more iterations per client to achieve statistical significance\n", carg.iterations);
		}
	}

	// send termination signal
	sockfd = socket(AF_INET,SOCK_STREAM, 0);
	if (sockfd == -1) {
		// TODO:: handle socket error
	}

	ret = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in));
	if (ret == -1) {
		// TODO: handle connect error
	}

	ret = sendto(sockfd, &zero, sizeof(zero), 0, NULL, 0);
	if (ret == -1) {
		// TODO: handle errno
	}
	close(sockfd);


	printf("%.2fus\n", mean/1000.0);
	exit(EXIT_SUCCESS);
}

void * client(void * arg) {
	struct client_details * config = arg;
	struct client_stats * stats;
	double current_mean;

	struct sockaddr * servaddr;
	int sockfd;

	struct timespec start, end;
	time_t secdiff;
	long nsecdiff;
	double diff;

	uint32_t response, challenge = 0;

	long i;
	int ret;

	stats = malloc(sizeof(struct client_stats));
	memset(stats, 0, sizeof(struct client_stats));

	sockfd = socket(AF_INET,SOCK_STREAM, 0);
	if (sockfd == -1) {
		// TODO:: handle socket error
		return stats;
	}

	servaddr = (struct sockaddr *) config->servaddr;
	ret = connect(sockfd, servaddr, sizeof(struct sockaddr_in));

	if (ret == -1) {
		// TODO: handle connect error
		return stats;
	}

	for (i = 0; i < config->iterations; i++) {
		while (challenge == 0) {
			challenge = htonl(arc4random());
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start);
		ret = sendto(sockfd, &challenge, sizeof(challenge), 0, NULL, 0);
		if (ret == -1) {
			// TODO: handle errno
			return stats;
		}

		ret = recvfrom(sockfd, &response , sizeof(response), MSG_WAITALL, NULL, NULL);
		if (ret == -1) {
			// TODO: handle errno
			return stats;
		}
		if (ret == 0) {
			// TODO: handle connection closed
			return stats;
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end);

		challenge = ntohl(challenge);
		response = ntohl(response);
		if (response != challenge + 1) {
			fprintf(stderr, "server responded with incorrect response (%u != %u+1)\n", response, challenge);
			return stats;
		}

		secdiff = end.tv_sec - start.tv_sec;
		nsecdiff = end.tv_nsec - start.tv_nsec;
		diff = secdiff * 1e9 /* nanoseconds */;
		diff += nsecdiff;

		stats->n += 1;
		current_mean = stats->mean;
		stats->mean = stats->mean + (diff-stats->mean)/stats->n;
		stats->S = stats->S + (diff - stats->mean) * (diff - current_mean);
	}

	close(sockfd);
	return stats;
}
