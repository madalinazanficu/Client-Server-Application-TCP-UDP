#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include <iostream>
#include <netinet/tcp.h>

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc < 3) {
		usage(argv[0]);
	}

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	/* Multimea de socketi de citire folosita in select() */
	fd_set read_fds;
	FD_ZERO(&read_fds);

	/* Multime folosita temporar */
	fd_set tmp_fds;
	FD_ZERO(&tmp_fds);

	/* Valoare maxima fd din multimea read_fds */
	int fdmax;
	
	/* Socket TCP pentru conectarea la server */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	/* Disabling Nagle's algorithm */
	int flag = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(ret != 0, "Error Nagle's algorithm");

	/* Adresa serverului, familia de adrese si portul pentru conectare */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	/* Conexiunea catre server */
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	/* 0 = file descriptor pt STDIN */
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	/* Se trimite id-ul clientului curent catre server*/
	char client_id[BUFLEN];
	strcpy(client_id, argv[1]);
	int send_id = send(sockfd, client_id, strlen(client_id), 0);

	while (1) {
		// /* In tmp_fds se aleg conexiunile */
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "Error in client.c linia 75");

		/* Varianta 1: se citesc de la stdin comenzile:
			exit, subscribe sau unsubscribe
		*/
		if (FD_ISSET(0, &tmp_fds)) {
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			if (strncmp(buffer, "exit", 4) == 0) {
				printf("Client %s disconnected.\n", client_id);
				break;
			}
			int choose_message = 0;
			if (strncmp(buffer, "subscribe", 9) == 0) {
				choose_message = 1;
			}

			if (strncmp(buffer, "unsubscribe", 11) == 0) {
				choose_message = 2;
			}

			if (choose_message == 0) {
				fprintf(stderr, "Mesaj incorect");
			} else {
				/* Se trimite mesaj la server */
				n = send(sockfd, buffer, strlen(buffer), 0);
				DIE(n < 0, "send");

				/* Mesajul a fost valid si trimis cu succes la server */
				if (choose_message == 1) {
					printf("Subscribed to topic.\n");
				} else if (choose_message == 2) {
					printf("Unsubscribed from topic.\n");
				}
			}
		}

		/* Varianta 2: mesaj venit de la server */
		if (FD_ISSET(sockfd, &tmp_fds)) {
			memset(buffer, 0, sizeof(buffer));
			int bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
			DIE(bytes_received < 0, "Recv error in run_client");

			/* Mesaj de inchidere a serverului */
			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}
		}
	}

	/* Se inchide conexiunea si socketul creat */
	close(sockfd);

	return 0;
}
