#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>

#include "../checked.h"

static volatile sig_atomic_t sigintRecu = 0;
static volatile sig_atomic_t triggerCleanup = 0;

static void GestionnaireSigint([[ maybe_unused ]] int sigint) {
   sigintRecu = 1;  // doit tout clean et fermer le programme
}

static void GestionnaireSigpipe([[ maybe_unused ]] int sigpipe) {
    const char *message = "Déconnexion de l'interlocuteur détectée ou Pipe cassé \n";
    ssize_t bytesWr =  write(STDOUT_FILENO, message, sizeof(message) - 1);
    (void)bytesWr;
    triggerCleanup = 1;
    
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
    int server_fd = checked(socket (AF_INET , SOCK_STREAM , 0)); // Créer le socket
    
    int opt = 1;
    // Permet la réutilisation du port/de l'adresse
    setsockopt (server_fd , SOL_SOCKET , SO_REUSEADDR | SO_REUSEPORT , &opt , sizeof (opt ));
    struct sockaddr_in address ;
    address . sin_family = AF_INET ;
    address . sin_addr .s_addr = INADDR_ANY ;
    address . sin_port = htons (port);
    // Définit l'adresse et le port d'écoute , réserve le port
    checked(bind(server_fd , ( struct sockaddr *)& address , sizeof ( address )));

    // Commence l'écoute
    checked(listen (server_fd , 5)); // maximum 3 connexions en attente
    size_t addrlen = sizeof ( address );
    // Ouvre une nouvelle connexion
    int new_socket = checked(accept(server_fd , ( struct sockaddr *)& address , ( socklen_t *)& addrlen ));
    char buffer [1024];
    // Reçoit un message
    checked(read(new_socket , buffer , 1024));
   close(server_fd);
   close(new_socket);
}

int main(void) {
    sock_creation();
    return 0;
}
