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

#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>


#include "../checked.h"
#define MAX_CLIENTS 1000
static volatile sig_atomic_t sigint_recu = 0;
static volatile sig_atomic_t triggerCleanup = 0;


pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex pour synchroniser

struct ListeClient {
    int client_sockets[MAX_CLIENTS];  // Tableau pour stocker les sockets des clients
    int client_count;             // Nombre de clients connectés
    char* client_usernames[MAX_CLIENTS];
};

struct ListeClient liste_client; 

void printClientUsernames() {
    printf("Liste des usernames des clients :\n");
    for (int i = 0; i < liste_client.client_count; i++) {
        printf("Client %d: Username=%s\n", i + 1, liste_client.client_usernames[i]);
    }
}

static void GestionnaireSigint([[ maybe_unused ]] int sigint) {
   sigint_recu = 1;  // doit tout clean et fermer le programme
}

void shutdown_server(int server_fd){
    printf(" serveur se ferme \n ");
    pthread_mutex_lock(&clients_mutex);
    const char* server_shutdown = " Déconnexion du serveur. \n";
    for (int i = 0; i < liste_client.client_count; i++) {
        send(liste_client.client_sockets[i], server_shutdown, strlen(server_shutdown), 0); 
        // envoie un msg a chaque client que le serveur se déconnecte
        if(liste_client.client_usernames[i]){
            free(liste_client.client_usernames[i]);
        }
        close(liste_client.client_sockets[i]);
    }
    close(server_fd);
    liste_client.client_count = 0;
    pthread_mutex_unlock(&clients_mutex);

    pthread_mutex_destroy(&clients_mutex); // libere la memoire du mutex
} 

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

bool no_space(const char * buffer){
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ' '){
            printf("ya un espace donc pas username \n");
            return false;
        }
    }
    printf("pas d'espace : username on doit add\n");
    return true;
}

bool add_username(char * buffer){
    pthread_mutex_lock(&clients_mutex);

    if (liste_client.client_count >= MAX_CLIENTS){
        printf("trop de client \n");
        pthread_mutex_unlock(&clients_mutex); // si plus d'espace ret false
        return false;
    }
    if (!no_space(buffer)) {
        printf(" dans func add username ya un espace \n");
        pthread_mutex_unlock(&clients_mutex);
        return false; // Le buffer contient un espace on ajoute pas 
    }
    liste_client.client_usernames[liste_client.client_count] = strdup(buffer); // alloue mem
    // ajoute un username si espace libre
    if (!liste_client.client_usernames[liste_client.client_count]) {
        perror("Échec de l'allocation mémoire");
        pthread_mutex_unlock(&clients_mutex);
        return false;
    }
    printf("Username ajouté : %s\n", buffer);
    liste_client.client_count++; // incremente le nombre de client
    printClientUsernames();

    pthread_mutex_unlock(&clients_mutex);
    return true;

}

// Ajouter un username à la liste des clients
void add_username2(char * buffer) {
    pthread_mutex_lock(&clients_mutex);
    if (liste_client.client_count < MAX_CLIENTS) {
        if (no_space(buffer)) {
            liste_client.client_usernames[liste_client.client_count] = strdup(buffer); // Fixe un bug : utilisation de strdup
        }
    }
    printClientUsernames();
    pthread_mutex_unlock(&clients_mutex);
}

char* get_username(char *buffer) {
    // osef juste faire strok du premier elem
    char *username = (char*)malloc(30 * sizeof(char)); // username max 30 octets
    if (!username) {
        perror("Erreur malloc \n");
        exit(EXIT_FAILURE);
    }
    int i = 0;
    while (buffer[i] != '\0' && buffer[i] != ' ') {
        username[i] = buffer[i];
        i++;
    }
    username[i] = '\0';
    return username;
}


void *handle_client(void *client_sock) {
    int sock = *(int *)client_sock;
    free(client_sock); // client_sock est inutile à présent libere 
    char buffer[1024];
    while (!sigint_recu) {
   
        ssize_t bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0'; // evite des bug
            add_username2(buffer);
            printf("le buffer : %s \n", buffer);
            fflush(stdout);
            char *recipient = strtok(buffer, " "); // get username
            char *message = strtok(NULL, "");
            printf(" msg and destinaire %s  / %s \n", message, recipient);
            fflush(stdout);
            pthread_mutex_lock(&clients_mutex);
            if (recipient && message) {
    

                // Find the recipient in the list
                int recipient_sock = -1;

                for (int i = 0; i < liste_client.client_count; i++) {
                    if (liste_client.client_usernames[i] != NULL &&
                    strcmp(liste_client.client_usernames[i], recipient) == 0) {
                        recipient_sock = liste_client.client_sockets[i];
                        printf("%i\n",recipient_sock);
                        break ;
                    }
                }

                if (recipient_sock != -1) {
                    // Debug: print recipient and message
                    printf("Sending message to recipient socket %d: %s\n", recipient_sock, message);
                    ssize_t sent_bytes = send(recipient_sock, message, strlen(message), 0);
                    if (sent_bytes == -1) {
                        perror("Error sending message to recipient");
                    } else {
                        printf("Message envoyé à %s : %s\n", recipient, message);
                    }
                }
                else {
                    // Notify sender that the recipient was not found
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg), "Cette personne (%s) n'est pas connectée.\n", recipient);
                    const char *err_msg = "Recipient not found\n";
                    // envoie a l'utilisateur le msg d'erreur
                    ssize_t send_bytes = send(sock, err_msg, strlen(err_msg), 0); 
                    if (send_bytes == -1) {
                        perror("Erreur lors de l'envoi du message d'erreur à l'expéditeur");
                    }
                }
            }

            pthread_mutex_unlock(&clients_mutex);

        }else if (bytes_read == 0) { // Le client a fermé la connexion
    // SIGINT DOIT DECO TLMD 
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
        }
        else if (bytes_read < 0){
            perror("erreur lord de la lecture");
            // faire fonction cleanup
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

    liste_client.client_count = 0; // initialise à 0

    int server_fd = checked(socket (AF_INET , SOCK_STREAM , 0)); // Créer le socket
    int opt = 1;
    // Permet la réutilisation du port/de l'adresse
    if (setsockopt (server_fd , SOL_SOCKET , SO_REUSEADDR | SO_REUSEPORT , &opt , sizeof (opt )) < 0){
        perror("setsock");
    }
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
    while (!sigint_recu) {
        
        fflush(stdout);
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        printf(" Nouvelle connexion acceptée : socket %d\n", new_socket);
        fflush(stdout);
        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            if (client_sock == NULL) {
                perror("malloc failed");
                close(new_socket);
                continue; // passe au prochain client
            }
            
            *client_sock = new_socket;
            add_client(new_socket);

            pthread_t tid;
            if (pthread_create(&tid, NULL, handle_client, client_sock)!= 0) {
                perror("Erreur lors de la création du thread client");
                close(*client_sock);
                free(client_sock);
                continue;
            } // Passe au client suivant

           
                //pthread_detach(tid); // Automatically free thread resources
                //free(client_sock);

            pthread_detach(tid); // Automatically free thread resources
        }

    }
    shutdown_server(server_fd);

}

int main(void) {
    liste_client.client_count = 0; 
    signal(SIGINT, GestionnaireSigint);
    signal(SIGPIPE, SIG_IGN);
    sock_creation();
    return 0;

}
