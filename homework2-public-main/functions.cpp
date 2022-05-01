#include "functions.h"

void connect_client(int cl_socket, char client_id[BUFLEN], char ip_address[BUFLEN],
					uint16_t port,  std::vector<struct client_t> &all_clients) {
	bool connected = false;
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		/* Clientul este deja inregistrat */
		if (cl->id == client_id) {
			// connected = true;
			if (cl->active == true) {
				printf("Client %s already connected.\n", client_id);
			} else{
				cl->active = true;

				/* Se adauga tot ce avea el in coada*/
			}
		}
	}
	if (connected == false) {
		struct client_t new_client;
		new_client.active = 1;
		new_client.socket = cl_socket;
		strcpy(new_client.id, client_id);
		all_clients.push_back(new_client);
		printf("New client %s connected from %s:%d\n", client_id, ip_address, port);
	}
}

void disconnect_all_clients(std::vector<struct client_t> &all_clients) {
	/* Deconectarea tuturor clientilor activi */
	for (auto cl = all_clients.begin(); cl != all_clients.end(); cl++) {
		/* Se trimite cerere de inchidere clientilor */
		char buffer[256];
		memset(buffer, 0, BUFLEN);
		strcpy(buffer, "exit");
		int res = send(cl->socket, buffer, strlen(buffer), 0);
		close(cl->socket);
		all_clients.erase(cl);
	}
}