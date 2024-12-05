#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    int new_socket = accept(server_fd , ( struct sockaddr *)& address , ( socklen_t *)& addrlen );
    char buffer [1024];
    // Reçoit un message
    read(new_socket , buffer , 1024);}

int main(void) {
    sock_creation();
}