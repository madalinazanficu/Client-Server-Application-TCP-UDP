#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include "functions.h"
#include <iostream>
#include <netinet/tcp.h>
#include <unordered_map>
#include <vector>

/* int  -> socketul clientului 
   bool -> True -> 	client conectat /False -> client neconectat */
std::unordered_map<int, bool> clients;

/* int    -> socketul clientului 
   char * -> id-ul clientului  */
std::unordered_map<int, char*> clients_id;

std::vector<struct client_t> all_clients;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	if (argc < 2) {
		usage(argv[0]);
	}

	int sockfd, newsockfd, portno;
	int udp_socket;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;
	char client_id[256];

	/* Multimea de socketi de citire folosita in select() */
	fd_set read_fds;

	/* Multime folosita temporar */
	fd_set tmp_fds;

	/* Valoare maxima fd din multimea read_fds */
	int fdmax;

	/* Se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds) */
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	/* Socket TCP pentru conectarea la clienti */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "tcp socket");

	/* Socket UDP pentru primirea datagramelor */
	udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  	DIE(udp_socket < 0, "udp socket");


	/* Disabling Nagle's algorithm */
	int flag = 1;
    ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
    DIE(ret != 0, "Error Nagle's algorithm");

	/* Adresa serverului, familia de adrese si portul pentru conectare */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");


	/* Se adauga noii file descriptors (socketul pe care
	   se asculta conexiuni TCP si socketul pe care se sculta
	   conexiuni UDP) in multimea read_fds */
	FD_SET(sockfd, &read_fds);
	FD_SET(udp_socket, &read_fds);
	FD_SET(0, &read_fds);
	fdmax = std::max(sockfd, udp_socket);

	while (1) {
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		/* Verificare primire exit de la stdin => inchiderea tuturor clientilor activi */
		if (FD_ISSET(0, &tmp_fds)) {
			memset(buffer, 0, BUFLEN);
			n = read(0, buffer, sizeof(buffer) - 1);
			if (strncmp(buffer, "exit", 4) == 0) {
				disconnect_all_clients(all_clients);
				break;
			}
		}

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					/* Cerere de conexiune pe socketul inactiv de TCP */
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					/* Se adauga noul socket la multimea descriptorilor */
					FD_SET(newsockfd, &read_fds);
					fdmax = std::max(fdmax, newsockfd);

					/* Se retine id-ul noului client gasit */
					memset(client_id, 0, BUFLEN);
					n = recv(newsockfd, client_id, sizeof(client_id) - 1, 0);
					clients_id[newsockfd] = client_id;

					/* Conectarea unui nou client TCP */
					connect_client(newsockfd, client_id, inet_ntoa(cli_addr.sin_addr),
									ntohs(cli_addr.sin_port), all_clients);
								
				} else if (i == udp_socket) {
					/* Datagrama de la clientul UDP */
					memset(buffer, 0, BUFLEN);
					n = recvfrom(udp_socket, buffer, sizeof(struct datagram_t), 0,
								(struct sockaddr *) &cli_addr, &clilen);
					DIE(n < 0, "Invalid datagram");

					

				} else {
					/* S-au primit date pe unul din socketii de client,
						asa ca serverul trebuie sa le receptioneze */
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer) - 1, 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						/* Conexiunea s-a inchis */
						printf("Client %s disconnected.\n", clients_id[i]);
						close(i);
						
						/* Se scoate din multimea de citire socketul inchis */
						FD_CLR(i, &read_fds);
					} else {
						// int what_client = atoi(buffer);
						// // printf("in server.c: %d\n", what_client);
						// int q = send(what_client, buffer, strlen(buffer) + 1, 0);
						// DIE(q < 0, "Error in server linia 105");
					}
				}
			}
		}
	}

	close(sockfd);
	return 0;
}