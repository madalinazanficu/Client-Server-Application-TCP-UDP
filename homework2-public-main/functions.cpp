#include "functions.h"


void notify_clients(std::vector<struct client_t> &all_clients,
                    char topic[50], struct packet_tcp_t *tcp_packet,
                    std::unordered_map<int, std::queue <struct packet_tcp_t>> &waiting_queue) {

	/* Pentru fiecare client, verific daca se regaseste topicul curent */
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::unordered_map<std::string, bool> subscriptions = cl->subscriptions;

		if (subscriptions.find(topic) != subscriptions.end()) {
			if (cl->active == 1) {
				/* Client activ */
				int n = send(cl->socket, (char *)tcp_packet, BUFLEN, 0);
				DIE(n < 0, "Error notifing clients");
			} else {
				/* Client inactiv */
				int cl_socket = cl->socket;

				/* 2 tipuri de clienti in functie de sf */
				if (cl->sf == 1) {

					/* Initializare coada pentru clientul curent */
					if (waiting_queue.find(cl_socket) == waiting_queue.end()) {
						std::queue<struct packet_tcp_t> q;
						waiting_queue[cl_socket] = q;
					}
					waiting_queue[cl_socket].push(*tcp_packet);
			
				} else if (cl->sf == 0) {
					fprintf(stderr, "Clientul nu permite pastrarea in coada");
				}
			}
		}
	}
}

void from_udp_to_tcp(char buffer[1600], char ip_address[16],
					uint16_t port, struct packet_tcp_t* tcp_packet) {

	/* Convert the received data to datagram type */
	struct datagram_udp_t *datagram_udp = (struct datagram_udp_t *) buffer;

	/* Complete the fields of the packet */
	tcp_packet->type = datagram_udp->type;
	tcp_packet->udp_sender_port = port;
	memcpy(tcp_packet->upd_sender_ip, ip_address, strlen(ip_address));
	memcpy(tcp_packet->topic, datagram_udp->topic, sizeof(datagram_udp->topic));
	
	bool check = false;
	if (datagram_udp->type == 0) {
		/* extract the number */
		int int_number = *(uint32_t *)(datagram_udp->payload + 1);
		int_number = ntohl(int_number);

		/* check the sign */
		if (datagram_udp->payload[0] == 1) {
			int_number = -int_number;
		}
		sprintf(tcp_packet->payload, "%d", int_number);
		check = true;
	}
	if (datagram_udp->type == 1) {
		/* extract the number */
		uint16_t short_number = *(uint16_t *)datagram_udp->payload;
		short_number = ntohs(short_number);
		float sh_number = (short_number * 1.0) / 100;

		sprintf(tcp_packet->payload, "%.2f", sh_number);
		check = true;
	}
	if (datagram_udp->type == 2) {
		/* extract the number */
		float fl_number = *(uint32_t *)(datagram_udp->payload + 1);
		fl_number = ntohl(fl_number);

		uint8_t p = *(uint8_t *)(datagram_udp->payload + sizeof(uint32_t) + 1);
		fl_number = fl_number / pow(10, p);
		
		/* check the sign */
		if (datagram_udp->payload[0] == 1) {
			fl_number = -fl_number;
		}
		sprintf(tcp_packet->payload, "%f", fl_number);
		check = true;
	}
	if (datagram_udp->type == 3) {
		memcpy(tcp_packet->payload, datagram_udp->payload, sizeof(datagram_udp->payload));
		check = true;
	}
	// printf("Topic: %s, Type: %d, Msg: %s\n", tcp_packet->topic, tcp_packet->type, tcp_packet->payload);
	if (check == false) {
		fprintf(stderr, "Invalid type.");
	}				
}

void send_from_queue(int cl_socket,
					std::unordered_map<int, std::queue<packet_tcp_t>> &waiting_queue) {

	while (!waiting_queue[cl_socket].empty()) {
		packet_tcp_t packet = waiting_queue[cl_socket].front();
		waiting_queue[cl_socket].pop();

		int n = send(cl_socket, (char *)&packet, BUFLEN, 0);
		DIE(n < 0, "Error sending from queue");
	}
}

void connect_client(int cl_socket, char client_id[BUFLEN], char ip_address[BUFLEN],
					uint16_t port,  std::vector<struct client_t> &all_clients,
					std::unordered_map<int, std::queue<packet_tcp_t>> &waiting_queue) {

	bool connected = false;
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {

		/* Clientul este deja inregistrat */
		if (strncmp(cl->id, client_id, 10) == 0) {
			connected = true;
			if (cl->active == true) {
				printf("Client %s already connected.\n", client_id);
			} else {
				printf("New client %s connected from %s:%d\n", client_id, ip_address, port);
				cl->active = true;

				/* Se trimite tot ce avea el in coada de asteptare */
				send_from_queue(cl->socket, waiting_queue);
			}
		}
	}
	if (connected == false) {
		std::unordered_map<std::string, bool> subscriptions;
		struct client_t new_client;
		new_client.subscriptions = subscriptions;
		new_client.active = 1;
		new_client.socket = cl_socket;
		new_client.sf = -1;
		strcpy(new_client.id, client_id);

		all_clients.push_back(new_client);
		printf("New client %s connected from %s:%d\n", client_id, ip_address, port);
	}
}

void disconnect_all_clients(std::vector<struct client_t> &all_clients) {
	/* Deconectarea tuturor clientilor activi */
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		/* Se trimite cerere de inchidere clientilor */
		char buffer[BUFLEN];
		memset(buffer, 0, BUFLEN);
		strcpy(buffer, "exit");
		int res = send(cl->socket, buffer, sizeof(buffer), 0);
		DIE(res < 0, "Send was compromised!");
		close(cl->socket);
	}
	all_clients.clear();
}

void change_sf(std::string &client_id, int sf,
					std::vector<struct client_t> &all_clients) {
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::string check_id(cl->id);

		if (check_id == client_id) {
			cl->sf = sf;
		}
	}
}

void subscribe_user(std::string &client_id, char* topic,
					std::vector<struct client_t>&all_clients) {

	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::string check_id(cl->id);

		if (client_id == check_id) {
			cl->subscriptions[topic] = true;
		}
	}
}

void unsubscribe_user(std::string &client_id, char* topic,
					std::vector<struct client_t>& all_clients) {
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::string check_id(cl->id);

		if (client_id == check_id) {
			cl->subscriptions[topic] = false;
		}
	}
}