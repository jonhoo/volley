#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>

#include <stdatomic.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

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
const double Z = 1.96; // 95% probability estimated value
const double E = 5000; // lies within +/- 5us of true value
const int MAX_ITERATIONS_PER_ROUND = 1000000;
static const int ONE = 1;

static atomic_int wait_n = 0;

int main(int argc, char** argv) {
	int port, clients = 0;
	int opt, i, ret;
	long max_iterations;
	int sockfd;
	int failed = 0;

	struct sockaddr_in servaddr;
	struct client_details carg;
	pthread_t * threads;
	uint32_t zero = 0;

	struct client_stats *cret;
	double current_mean, mean = 0;
	double stddev = 0;
	long n = 0;

	struct timespec start, now;
	time_t secdiff;

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

	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	for (; carg.iterations > 10;) {

		atomic_store(&wait_n, clients);
		for (i = 0; i < clients; i++) {
			ret = pthread_create(&threads[i], NULL, client, &carg);
			if (ret != 0) {
				errno = ret;
				perror("failed to spawn worker thread");
				goto after;
			}
		}

		for (i = 0; i < clients; i++) {
			ret = pthread_join(threads[i], (void**) &cret);

			if (ret != 0) {
				errno = ret;
				perror("failed to join worker thread");
				failed = 1;
			}
			if (cret == PTHREAD_CANCELED) {
				fprintf(stderr, "thread cancelled...\n");
				failed = 1;
			}
			if (cret == NULL && failed == 0) {
				fprintf(stderr, "thread aborted...\n");
				failed = 1;
			}
			if (failed == 1) continue;

			current_mean = mean;
			mean = (n * current_mean + cret->n * cret->mean) / (n + cret->n);
			stddev = sqrt((pow(stddev, 2)*n+cret->S)/(n+cret->n));
			n += cret->n;
			free(cret);
		}

		carg.iterations = (long) ceil((pow((Z * stddev) / E, 2) - n) / clients);
		fprintf(stderr, "iteration complete: %.0fus/%.2fus\n", mean/1000.0, stddev/1000.0);
		if (carg.iterations > max_iterations) {
			fprintf(stderr, "need many more samples (%ld) to achieve statistical significance, doing another %ld per client\n", carg.iterations * clients, max_iterations);
			carg.iterations = max_iterations;
		} else if (carg.iterations > 0) {
			fprintf(stderr, "running %ld more iterations per client to achieve statistical significance\n", carg.iterations);
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		secdiff = now.tv_sec - start.tv_sec;

		if (secdiff > 5*60 && carg.iterations > 10) {
			fprintf(stderr, "we've been spinning for too long -- giving up\n");
			break;
		}
	}

after:

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

	if (failed == 1) {
		return EXIT_FAILURE;
	}

	printf("%.0fus %.2fus %ld\n", mean/1000.0, stddev/1000.0, n);
	return EXIT_SUCCESS;
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

	long i = 0;
	int ret;

	stats = malloc(sizeof(struct client_stats));
	memset(stats, 0, sizeof(struct client_stats));

	sockfd = socket(AF_INET,SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("failed to open outgoing socket");
		atomic_store(&wait_n, -1);
		free(stats);
		return NULL;
	}

	servaddr = (struct sockaddr *) config->servaddr;
	ret = connect(sockfd, servaddr, sizeof(struct sockaddr_in));

	if (ret != 0) {
		perror("failed to connect to server");
		atomic_store(&wait_n, -1);
		free(stats);
		return NULL;
	}

	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &ONE, sizeof(ONE));

	// verify connection

	challenge = htonl(1);

	do {
		ret = sendto(sockfd, &challenge, sizeof(challenge), 0, NULL, 0);
	} while (ret == -1 && errno == EAGAIN);

	if (ret == -1) {
		perror("failed to send test challenge to server");
		atomic_store(&wait_n, -1);
		goto thread_after;
	}

	do {
		ret = recvfrom(sockfd, &response , sizeof(response), MSG_WAITALL, NULL, NULL);
	} while (ret == -1 && errno == EAGAIN);

	if (ret == -1) {
		if (errno == ECONNRESET) {
			fprintf(stderr, "reconnecting...\n");
			free(stats);
			return client(arg);
		}
		perror("failed to receive test response from server");
		goto thread_after;
	}
	if (ret == 0) {
		perror("received no test data from server; connection closed");
		goto thread_after;
	}

	response = ntohl(response);
	if (response != 2) {
		fprintf(stderr, "server responded with incorrect test response (%u != 2)\n", response);
		goto thread_after;
	}

	atomic_fetch_sub(&wait_n, 1);
	while (atomic_load(&wait_n) > 0) usleep(1);

	if (atomic_load(&wait_n) < 0) {
		goto thread_after;
	}

	fprintf(stderr, "thread connection ready.\n");
	for (i = 0; i < config->iterations; i++) {
		while (challenge == 0) {
			challenge = htonl(arc4random());
		}

		clock_gettime(CLOCK_MONOTONIC_RAW, &start);

		do {
			ret = sendto(sockfd, &challenge, sizeof(challenge), 0, NULL, 0);
		} while (ret == -1 && errno == EAGAIN);

		if (ret == -1) {
			perror("failed to send challenge to server");
			break;
		}

		do {
			ret = recvfrom(sockfd, &response , sizeof(response), MSG_WAITALL, NULL, NULL);
		} while (ret == -1 && errno == EAGAIN);

		if (ret == -1) {
			perror("failed to receive response from server");
			break;
		}
		if (ret == 0) {
			perror("received no data from server; connection closed");
			break;
		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &end);

		challenge = ntohl(challenge);
		response = ntohl(response);
		if (response != challenge + 1) {
			fprintf(stderr, "server responded with incorrect response (%u != %u+1)\n", response, challenge);
			break;
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

thread_after:
	close(sockfd);

	if (i < config->iterations-1) {
		atomic_store(&wait_n, -1);
		free(stats);
		return NULL;
	}
	return stats;
}
