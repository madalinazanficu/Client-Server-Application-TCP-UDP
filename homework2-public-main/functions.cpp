#include "functions.h"

/* Notificarea clientilor despre aparitia unui nou messaj
   Client activ abondat la topicul specific => se trimite pachetul aferent
   Client inactiv abonat (cu sf = 1) pachetele sunt pastrate in coada pana la
   reconectarea clientului.
   Client inactiv abondat (cu sf = 0) pachetele sunt aruncate.

*/
void notify_clients(std::vector<struct client_t> &all_clients,
                    char topic[50], struct packet_tcp_t *tcp_packet,
                    std::unordered_map<int, std::queue <struct packet_tcp_t>> &waiting_queue) {

	/* Pentru fiecare client, verific daca se regaseste topicul curent */
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::unordered_map<std::string, int> subscriptions = cl->subscriptions;
		if (subscriptions.find(topic) != subscriptions.end()) {
			if (cl->active == 1) {
				int n = send(cl->socket, (char *)tcp_packet, BUFLEN, 0);
				DIE(n < 0, "Error notifing clients");
			} else {
				int cl_socket = cl->socket;
				int active_sf = subscriptions[topic];
				if (active_sf == 1) {

					/* Initializare coada pentru clientul curent */
					if (waiting_queue.find(cl_socket) == waiting_queue.end()) {
						std::queue<struct packet_tcp_t> q;
						waiting_queue[cl_socket] = q;
					}
					/* Adaugarea in coada a pechetului */
					waiting_queue[cl_socket].push(*tcp_packet);
				} else if (active_sf == 0) {
					fprintf(stderr, "Clientul nu permite pastrarea in coada");
				}
			}
		}
	}
}

void send_from_queue(int cl_socket,
					std::unordered_map<int,
					std::queue<packet_tcp_t>> &waiting_queue) {

	while (!waiting_queue[cl_socket].empty()) {
		packet_tcp_t packet = waiting_queue[cl_socket].front();
		waiting_queue[cl_socket].pop();

		int n = send(cl_socket, (char *)&packet, BUFLEN, 0);
		DIE(n < 0, "Error sending from queue");
	}
}

/*	Inregistrarea unui nou client.
	Reconectarea unui client deja aflat in sistem.
	Verificarea ca 2 clienti cu acelasi id sa nu fie conectati simultan.
	Trimiterea pachetelor acumulate de catre un client inactiv.
*/

void connect_client(fd_set *read_fds, int cl_socket, char client_id[BUFLEN], char ip_address[BUFLEN],
					uint16_t port,  std::vector<struct client_t> &all_clients,
					std::unordered_map<int, std::queue<packet_tcp_t>> &waiting_queue) {

	bool connected = false;
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		/* Clientul este deja inregistrat */
		if (strncmp(cl->id, client_id, 10) == 0) {
			connected = true;
			/* Client cu acelasi id */
			if (cl->active == true) {
				printf("Client %s already connected.\n", client_id);
				disconnect_one_client(cl_socket, read_fds);
			} else {
				/* Reactivarea unui client vechi => se trimit pachetele din coada */
				printf("New client %s connected from %s:%d\n", client_id, ip_address, port);
				cl->active = true;
				send_from_queue(cl->socket, waiting_queue);
			}
		}
	}
	/* Inregistrarea unui nou client */
	if (connected == false) {
		std::unordered_map<std::string, int> subscriptions;
		struct client_t new_client;
		new_client.subscriptions = subscriptions;
		new_client.active = 1;
		new_client.socket = cl_socket;
		strcpy(new_client.id, client_id);
		all_clients.push_back(new_client);
		printf("New client %s connected from %s:%d\n", client_id, ip_address, port);
	}
}

/* Deconectarea tuturor clientilor activi 
   Utilizata pentru cazul de exit al serverului */
void disconnect_all_clients(std::vector<struct client_t> &all_clients, fd_set *read_fds) {
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		disconnect_one_client(cl->socket, read_fds);
	}
	all_clients.clear();
}

void disconnect_one_client(int cl_socket, fd_set *read_fds) {
	char buffer[BUFLEN];
	memset(buffer, 0, BUFLEN);
	strcpy(buffer, "exit");

	/* Se trimite cerere de inchidere clientului */
	int res = send(cl_socket, buffer, sizeof(buffer), 0);
	DIE(res < 0, "Send was compromised!");
	close(cl_socket);
	FD_CLR(cl_socket, read_fds);
}

/*  Adaugarea topicului in map-ul de subscribe.
	ACtivarea componenetei de storage (SF) specifica
    fiecarui topic din map-ul de subscribe */
void subscribe_user(std::string &client_id, char* topic, int new_sf,
					std::vector<struct client_t>&all_clients) {

	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::string check_id(cl->id);

		if (client_id == check_id) {
			if (new_sf == 1) {
				cl->subscriptions[topic] = 1;
			} else {
				cl->subscriptions[topic] = 0;
			}
		}
	}
}
/* Stergerea topicului din map-ul de subscribe */
void unsubscribe_user(std::string &client_id, char* topic,
					std::vector<struct client_t>& all_clients) {
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		std::string check_id(cl->id);

		if (client_id == check_id) {
			cl->subscriptions.erase(cl->subscriptions.find(topic));
		}
	}
}
/* Datele sunt primite in buffer
   Sunt convertite in functie de type si
   integrate in noua structura ce urmeaza a fi trimisa catre clienti
*/
void from_udp_to_tcp(char buffer[1600], char ip_address[16],
					uint16_t port, struct packet_tcp_t* tcp_packet) {

	/* Se convertesc datele primite */
	struct datagram_udp_t *datagram_udp = (struct datagram_udp_t *) buffer;
	tcp_packet->type = datagram_udp->type;
	tcp_packet->udp_sender_port = port;
	memcpy(tcp_packet->upd_sender_ip, ip_address, strlen(ip_address));
	memcpy(tcp_packet->topic, datagram_udp->topic, sizeof(datagram_udp->topic));
	
	bool check = false;
	if (datagram_udp->type == 0) {
		/* Extragerea si convertirea numarului din datagrama */
		int int_number = ntohl(*(uint32_t *)(datagram_udp->payload + 1));

		/* Verificare semn */
		if (datagram_udp->payload[0] == 1) {
			int_number = -int_number;
		}
		sprintf(tcp_packet->payload, "%d", int_number);
		check = true;
	}
	if (datagram_udp->type == 1) {
		/* Extragerea si convertirea numarului din datagrama */
		uint16_t short_number = ntohs(*(uint16_t *)datagram_udp->payload);
		float sh_number = (short_number * 1.0) / 100;

		sprintf(tcp_packet->payload, "%.2f", sh_number);
		check = true;
	}
	if (datagram_udp->type == 2) {
		/* Extragerea si convertirea numarului din datagrama */
		float fl_number = ntohl(*(uint32_t *)(datagram_udp->payload + 1));

		uint8_t p = *(uint8_t *)(datagram_udp->payload + sizeof(uint32_t) + 1);
		fl_number = fl_number / pow(10, p);
		
		/* Verificare semn */
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
	if (check == false) {
		fprintf(stderr, "Invalid type.");
	}				
}