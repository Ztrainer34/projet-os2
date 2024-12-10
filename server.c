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

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "../checked.h"
#define MAX_CLIENTS 1000
static volatile sig_atomic_t sigintRecu = 0;
static volatile sig_atomic_t triggerCleanup = 0;

int client_sockets[MAX_CLIENTS];  // Tableau pour stocker les sockets des clients
int client_count = 0;             // Nombre de clients connectés
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex pour synchroniser


static void GestionnaireSigint([[ maybe_unused ]] int sigint) {
   sigintRecu = 1;  // doit tout clean et fermer le programme
}
static void GestionnaireSigpipe([[ maybe_unused ]] int sigpipe) {
    const char *message = "Déconnexion de l'interlocuteur détectée ou Pipe cassé \n";
    ssize_t bytesWr =  write(STDOUT_FILENO, message, sizeof(message) - 1);
    (void)bytesWr;
    triggerCleanup = 1;

}
void add_client(int client_sock) {
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
        client_sockets[client_count++] = client_sock;
    } else {
        printf("Max clients reached. Connection refused.\n");
        close(client_sock);
    }
    pthread_mutex_unlock(&clients_mutex);
}


void *handle_client(void *client_sock) {
    int sock = *(int *)client_sock;
    free(client_sock);

    char buffer[1024];
    while (1) {
    int bytes_read = read(sock, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        printf("Client says: %s\n", buffer);
        send(sock, "Message received!", strlen("Message received!"), 0);
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] != sock) {
                send(client_sockets[i], buffer, strlen(buffer), 0);
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        pthread_mutex_lock(&clients_mutex);
        for (int i = 0; i < client_count; i++) {
            if (client_sockets[i] == sock) {
                client_sockets[i] = client_sockets[client_count - 1]; // Remplacer par le dernier client
                client_count--;
                break;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
    }
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
    fflush(stdout);
    // Définit l'adresse et le port d'écoute , réserve le port
    checked(bind(server_fd , ( struct sockaddr *)& address , sizeof ( address )));

    // Commence l'écoute
    checked(listen (server_fd , 5)); // maximum 3 connexions en attente
    size_t addrlen = sizeof ( address );
    // Ouvre une nouvelle connexion


    while (1) {
        int new_socket = checked(accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen));

        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            add_client(new_socket);
            *client_sock = new_socket;
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, client_sock);
            pthread_detach(tid); // Automatically free thread resources
        }

    }
   close(server_fd);

}

int main(void) {
    sock_creation();
    return 0;

}
