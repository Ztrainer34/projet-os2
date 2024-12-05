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
#include <pthread.h>

#include "../checked.h"

static volatile sig_atomic_t sigint_recu = 0;
static volatile sig_atomic_t trigger_cleanup = 0;

static void sigint_handler([[ maybe_unused ]] int sigint) {
   sigint_recu = 1;  // doit tout clean et fermer le programme
}

static void sigpipe_handler([[ maybe_unused ]] int sigpipe) {
    const char *message = "Déconnexion de l'interlocuteur détectée ou Pipe cassé \n";
    ssize_t bytesWr =  write(STDOUT_FILENO, message, sizeof(message) - 1);
    (void)bytesWr;
    trigger_cleanup = 1;
    
}

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

    while (1) {
      if (sigint_recu){
         printf("Signal SIGINT reçu, fermeture du programme \n");
         exit(EXIT_SUCCESS);
      }
        int new_socket = checked(accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen));
        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            *client_sock = new_socket;
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, client_sock);
            pthread_detach(tid); // Automatically free thread resources
        }
    
    } 
   close(server_fd);
   
}

int main(void) {
    struct sigaction sa;    //sigaction car sinn fgets bloquant
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0; // Pas de drapeau spécial
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Erreur lors de l'initialisation du gestionnaire SIGINT");
        exit(EXIT_FAILURE);
    }
    sock_creation();
    return 0;
}
