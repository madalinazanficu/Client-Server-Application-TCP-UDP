#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H 1

#include <unordered_map>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <iostream>
#include <netinet/tcp.h>
#include <vector>

typedef struct client_t client_t;
struct client_t {
    int socket;
    char id[256];
    bool active;
};

typedef struct datagram_t datagram_t;
struct datagram_t {
    char topic[51];
    uint8_t type;
    char content[1501];
};

typedef struct info_tcp_t info_tcp_t;
struct info_tcp_t {
    char topic[51];
    uint8_t type;
    char content[1501];
    uint16_t udp_sender_port;
    char upd_sender_ip[256];
};

void connect_client(int cl_socket, char client_id[BUFLEN], char ip_address[BUFLEN],
					uint16_t port,  std::vector<client_t> &all_clients);
void disconnect_all_clients(std::vector<struct client_t> &all_clients);


#endif