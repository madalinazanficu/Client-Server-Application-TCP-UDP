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
#include <iomanip>
#include <cmath>
#include <queue>

typedef struct client_t client_t;
struct client_t {
    int socket;
    char id[256];
    bool active;
    std::unordered_map<std::string, int> subscriptions;
};

typedef struct datagram_udp_t datagram_udp_t;
struct datagram_udp_t {
    char topic[50];
    uint8_t type;
    char payload[1500];
};

typedef struct packet_tcp_t packet_tcp_t;
struct packet_tcp_t {
    char topic[50];
    uint8_t type;
    char payload[1500];
    uint16_t udp_sender_port;
    char upd_sender_ip[16];
};

void from_udp_to_tcp(char buffer[1600], char ip_address[BUFLEN],
                    uint16_t port, struct packet_tcp_t *tcp_packet);

void connect_client(fd_set *read_fds, int cl_socket, char client_id[BUFLEN],
                    char ip_address[BUFLEN], uint16_t port, 
                    std::vector<struct client_t> &all_clients,
					std::unordered_map<int, std::queue<packet_tcp_t>> &waiting_queue);

void disconnect_one_client(int cl_socket, fd_set *read_fds);

void disconnect_all_clients(std::vector<struct client_t> &all_clients,
                            fd_set *read_fds);

void notify_clients(std::vector<struct client_t> &all_clients,
                    char topic[50], struct packet_tcp_t *tcp_packet,
                    std::unordered_map<int,
                    std::queue <struct packet_tcp_t>> &waiting_queue);

void subscribe_user(std::string &client_id, char* topic, int new_sf,
					std::vector<struct client_t>&all_clients);

void unsubscribe_user(std::string &client_id, char* topic,
					std::vector<struct client_t>& all_clients);

void change_sf(std::string &client_id, int sf,
               std::vector<struct client_t>& all_clients);

void send_from_queue(int cl_socket, std::unordered_map<int,
					std::queue<packet_tcp_t>> &waiting_queue);

void iterate_clients(std::vector<struct client_t> &all_clients);

#endif