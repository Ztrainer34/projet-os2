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

#define MAX_CLIENTS 1000

// Variables globales
static volatile sig_atomic_t sigintRecu = 0;
static volatile sig_atomic_t triggerCleanup = 0;

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex pour synchronisation

struct ListeClient {
    int client_sockets[MAX_CLIENTS];      // Tableau des sockets des clients
    int client_count;                     // Nombre de clients connectés
    char* client_usernames[MAX_CLIENTS];  // Tableau des usernames des clients
};

struct ListeClient liste_client;

// Fonction pour afficher les usernames des clients connectés
void printClientUsernames() {
    printf("Liste des usernames des clients :\n");
    for (int i = 0; i < liste_client.client_count; i++) {
        printf("Client %d: Username=%s\n", i + 1, liste_client.client_usernames[i]);
    }
}

// Gestionnaire de signal SIGINT (Ctrl+C)
static void GestionnaireSigint([[ maybe_unused ]] int sigint) {
    sigintRecu = 1; // Indique qu'il faut nettoyer et fermer le programme
}

// Gestionnaire de signal SIGPIPE (pipe cassé)
static void GestionnaireSigpipe([[ maybe_unused ]] int sigpipe) {
    const char *message = "Déconnexion de l'interlocuteur détectée ou Pipe cassé \n";
    ssize_t bytesWr = write(STDOUT_FILENO, message, strlen(message));
    (void)bytesWr;
    triggerCleanup = 1;
}

// Ajouter un socket client à la liste des clients
void add_client(int client_sock) {
    pthread_mutex_lock(&clients_mutex);
    if (liste_client.client_count < MAX_CLIENTS) {
        liste_client.client_sockets[liste_client.client_count++] = client_sock;
    } else {
        printf("Max clients reached. Connection refused.\n");
        close(client_sock);
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Fonction pour vérifier qu'une chaîne ne contient pas d'espaces
bool no_space(const char * buffer) {
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ' ') {
            return false;
        }
    }
    return true;
}

// Ajouter un username à la liste des clients
void add_username(char * buffer) {
    pthread_mutex_lock(&clients_mutex);
    if (liste_client.client_count < MAX_CLIENTS) {
        if (no_space(buffer)) {
            liste_client.client_usernames[liste_client.client_count] = strdup(buffer); // Fixe un bug : utilisation de strdup
        }
    }
    printClientUsernames();
    pthread_mutex_unlock(&clients_mutex);
}

// Fonction exécutée par chaque thread pour gérer un client
void *handle_client(void *client_sock) {
    int sock = *(int *)client_sock;
    free(client_sock); // pk on free

    char buffer[1024];
    while (1) {
        int bytes_read = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // Assure la terminaison de la chaîne
            add_username(buffer); // Ajoute un username dans la liste
            char *recipient = strtok(buffer, " ");
            char *message = strtok(NULL, "");



            if (recipient && message) {
                pthread_mutex_lock(&clients_mutex);

                // Find the recipient in the list
                int recipient_sock = -1;
                char *name ;
                for (int i = 0; i < liste_client.client_count; i++) {
                    if (liste_client.client_usernames[i+1] != NULL &&
                        strcmp(liste_client.client_usernames[i+1], recipient) == 0) {
                        recipient_sock = liste_client.client_sockets[i];
                        name = liste_client.client_usernames[i+1];
                        printf("%i\n",recipient_sock);
                        break;
                        }
                }

                if (recipient_sock != -1) {
                    // Debug: print recipient and message
                    printf("Sending message to recipient socket %d: %s\n", recipient_sock, name);
                    ssize_t sent_bytes = send(recipient_sock, message, strlen(message), 0);
                    if (sent_bytes == -1) {
                        perror("Error sending message to recipient");
                    } else {
                        printf("Message sent successfully (%ld bytes)\n", sent_bytes);
                    }
                }
                else {
                    // Notify sender that the recipient was not found
                    const char *err_msg = "Recipient not found\n";
                    send(sock, err_msg, strlen(err_msg), 0);
                }
            }

            pthread_mutex_unlock(&clients_mutex);
        } else if (bytes_read == 0) { // Le client a fermé la connexion
            printf("Client disconnected: socket %d\n", sock);
            close(sock);
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < liste_client.client_count; i++) {
                if (liste_client.client_sockets[i] == sock) {
                    liste_client.client_sockets[i] = liste_client.client_sockets[--liste_client.client_count];

                    free(liste_client.client_usernames[i]); // Libère la mémoire allouée pour le username
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
            break;
        }
    }
    return NULL;
}

// Fonction de création du socket serveur
void sock_creation() {
    const char *port_str = getenv("PORT_SERVEUR");
    int port = 1234; // Port par défaut
    if (port_str != NULL) {
        port = atoi(port_str); // Conversion en entier
        if (port < 1 || port > 65535) {
            port = 1234; // Réinitialisation au port par défaut
        }
    }

    liste_client.client_count = 0; // Initialisation à 0

    int server_fd = checked(socket(AF_INET, SOCK_STREAM, 0)); // Création du socket

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    fflush(stdout);

    checked(bind(server_fd, (struct sockaddr *)&address, sizeof(address))); // Liaison
    checked(listen(server_fd, 5)); // Écoute des connexions

    size_t addrlen = sizeof(address);
    while (1) {
        int new_socket = checked(accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen));
        if (new_socket >= 0) {
            int *client_sock = (int *)malloc(sizeof(int));
            if (client_sock == NULL) {
                perror("malloc failed");
                close(new_socket);
                continue;
            }
            *client_sock = new_socket;
            add_client(new_socket);
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, client_sock);
            pthread_detach(tid); // Libération automatique des ressources du thread
        }
    }
    close(server_fd);
}

// Fonction principale
int main(void) {
    signal(SIGINT, GestionnaireSigint);
    signal(SIGPIPE, GestionnaireSigpipe);
    sock_creation();
    return 0;
}
