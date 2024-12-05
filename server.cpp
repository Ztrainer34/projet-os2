#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>



void *handle_client(void *client_sock) {
    int sock = *(int *)client_sock;
    free(client_sock);

    char buffer[1024];
    int bytes_read = read(sock, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        printf("Client says: %s\n", buffer);
        send(sock, "Message received!", strlen("Message received!"), 0);
    }
    close(sock);
    return NULL;
}



void sock_creation(){
    const char *port_str = getenv("PORT_SERVEUR");
    int port = 1234; // Default port
    if (port_str != NULL) {
        port = atoi(port_str); // Convert to integer
        if (port < 1 || port > 65535) {
            port = 1234; // Reset to default if out of range
        }
    }
    int server_fd = socket (AF_INET , SOCK_STREAM , 0); // Créer le socket
    int opt = 1;
    // Permet la réutilisation du port/de l'adresse
    setsockopt (server_fd , SOL_SOCKET , SO_REUSEADDR | SO_REUSEPORT , &opt , sizeof (opt ));
    struct sockaddr_in address ;
    address . sin_family = AF_INET ;
    address . sin_addr .s_addr = INADDR_ANY ;
    address . sin_port = htons (port);
    // Définit l'adresse et le port d'écoute , réserve le port
    bind(server_fd , ( struct sockaddr *)& address , sizeof ( address ));

    // Commence l'écoute
    listen (server_fd , 5); // maximum 3 connexions en attente
    size_t addrlen = sizeof ( address );
    // Ouvre une nouvelle connexion
    while (1) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            *client_sock = new_socket;
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, client_sock);
            pthread_detach(tid); // Automatically free thread resources
        }
    }
}
int main(void) {
    sock_creation();
}
