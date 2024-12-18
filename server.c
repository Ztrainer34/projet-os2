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
pthread_mutex_t sigint_mutex = PTHREAD_MUTEX_INITIALIZER;
// mutex pour empecher acces simultanée des threads sur sigint_recu

int server_fd;
struct ListeClient {
    int client_sockets[MAX_CLIENTS];  // Tableau pour stocker les sockets des clients
    int client_count;             // Nombre de clients connectés
    pthread_t client_threads[MAX_CLIENTS];
    char* client_usernames[MAX_CLIENTS];
};

struct ListeClient liste_client;

void* gestionnaire_sigint_thread(void* set_){
    int sig;
    sigset_t *set = (sigset_t *)set_;

    if (sigwait(set, &sig) != 0) { // attend le bon signal
        perror("Erreur lors de l'attente du signal");

    }

    if (sig == SIGINT) {// Utilisez `shutdown` pour interrompre `accept`
    printf(" SERV FD handler %d \n", server_fd);
        if (shutdown(server_fd, SHUT_RDWR) < 0) {
            perror("Erreur lors du shutdown du serveur");
        }
        printf("Signal SIGINT reçu. Fermeture du serveur...\n");
        pthread_mutex_lock(&sigint_mutex);
        sigint_recu = 1; // Déclenche la fermeture
        pthread_mutex_unlock(&sigint_mutex);
    }

    return NULL;
}


void printClientUsernames() {
    printf("Liste des usernames des clients :\n");
    for (int i = 0; i < liste_client.client_count; i++) {
        printf("Client %d: Username=%s\n", i + 1, liste_client.client_usernames[i]);
    }
}

void shutdown_server(){
    printf(" serveur se ferme \n ");
    pthread_mutex_lock(&clients_mutex);
    const char* server_shutdown = " Déconnexion du serveur. \n";
    for (int i = 0; i < liste_client.client_count; i++) {
        send(liste_client.client_sockets[i], server_shutdown, strlen(server_shutdown), 0);
        // envoie un msg a chaque client que le serveur se déconnecte
        if(liste_client.client_usernames[i]){
            free(liste_client.client_usernames[i]);
            liste_client.client_usernames[i] = NULL; // Évitez une double libération
        }
        pthread_detach(liste_client.client_threads[i]);
        /**int ret = pthread_join(liste_client.client_threads[i], NULL);
        if (ret != 0) {
            printf("Erreur pthread_join pour le thread %d : %s\n", i + 1, strerror(ret));
        }*/
        close(liste_client.client_sockets[i]);

     }


    printf(" EFNIN FINI \n");
    close(server_fd);
    liste_client.client_count = 0;
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_destroy(&clients_mutex); // libere la memoire du mutex
    exit(EXIT_SUCCESS);
}

void add_client(int client_sock, pthread_t tid) {
    pthread_mutex_lock(&clients_mutex);
    if (liste_client.client_count < MAX_CLIENTS) {
        liste_client.client_threads[liste_client.client_count] = tid;
        liste_client.client_sockets[liste_client.client_count] = client_sock;
        liste_client.client_usernames[liste_client.client_count] = NULL; // Initialise à NULL
        liste_client.client_count++; // passe au prochain client

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


// Ajouter un username à la liste des clients
void add_username(char * buffer) {
    pthread_mutex_lock(&clients_mutex);
    int idx = liste_client.client_count - 1; // Dernier client ajouté
    if (liste_client.client_count < MAX_CLIENTS && liste_client.client_usernames[idx] == NULL) {
        if (no_space(buffer)) {
            liste_client.client_usernames[idx] = strdup(buffer); // Fixe un bug : utilisation de strdup
        }
    }
    printClientUsernames();
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *client_sock) {
    int sock = *(int *)client_sock;
    free(client_sock); // client_sock est inutile à présent libere
    char buffer[1024];
    while (!sigint_recu) {

        ssize_t bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read > 0) {

            buffer[bytes_read] = '\0'; // evite des bug
            add_username(buffer);

            char *recipient = strtok(buffer, " "); // get username
            char *message = strtok(NULL, "");
            char *username = NULL;

            if (recipient && message) {
                pthread_mutex_lock(&clients_mutex);

                // Find the recipient in the list
                int recipient_sock = -1;
                char *name ;
                for (int i = 0; i < liste_client.client_count; i++) {
                    // Find the recipient's socket based on recipient's username
                    if (liste_client.client_usernames[i] != NULL &&
                        strcmp(liste_client.client_usernames[i], recipient) == 0) {
                        recipient_sock = liste_client.client_sockets[i];
                        name = liste_client.client_usernames[i];
                        }

                    // Find the sender's username based on the socket
                    if (liste_client.client_sockets[i] == sock) {
                        username = liste_client.client_usernames[i];
                    }

                    // Break early if both are found
                    if (recipient_sock != -1 && username != NULL) {
                        break;
                    }
                }

                if (recipient_sock != -1) {
                    // Debug: print recipient and message
                    char formatted_message[1024];
                    snprintf(formatted_message, sizeof(formatted_message), "%s %s", username, message);
                    printf("Formatted message: %s\n", formatted_message);
                    ssize_t sent_bytes = send(recipient_sock, formatted_message, strlen(formatted_message), 0);
                    if (sent_bytes == -1) {
                        perror("Error sending message to recipient");
                    } else {
                        printf("Message envoyé à %s : %s\n", recipient, message);
                    }
                }
                else {
                    // Notify sender that the recipient was not found
                    char error_msg[64];
                    snprintf(error_msg, sizeof(error_msg), "Cette personne (%s) n'est pas connectée.\n", recipient);
                    //const char *err_msg = "Recipient not found\n"; // remplacer par cette prsn pas co
                    // envoie a l'utilisateur le msg d'erreur
                    ssize_t send_bytes = send(sock, error_msg, strlen(error_msg), 0);
                    if (send_bytes == -1) {
                        perror("Erreur lors de l'envoi du message d'erreur à l'expéditeur");
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
            }



        }else if (bytes_read == 0) { // Le client a fermé la connexion
    // SIGINT DOIT DECO TLMD
                printf("Client disconnected: socket %d\n", sock);
                close(sock);
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < liste_client.client_count; i++) {
                    if (liste_client.client_sockets[i] == sock) {
                                    // Vérifiez avant de libérer le username
                        if (liste_client.client_usernames[i] != NULL) {
                            free(liste_client.client_usernames[i]);
                            liste_client.client_usernames[i] = NULL; // Évitez une double libération
                        }

                        liste_client.client_sockets[i] = liste_client.client_sockets[--liste_client.client_count];

                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                break;
        }
        else if (bytes_read < 0){
            perror("Erreur lors de la lecture \n");
            // faire fonction cleanup
        }

    }
    printf(" HANDLE CLIENT FINI ?  \n");

    //close(sock);
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

    server_fd = checked(socket (AF_INET , SOCK_STREAM , 0)); // Créer le socket
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
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            if (client_sock == NULL) {
                perror("malloc failed");
                close(new_socket);
                continue; // passe au prochain client
            }

            *client_sock = new_socket;

            pthread_t tid;

            if (pthread_create(&tid, NULL, handle_client, client_sock)== 0) {
                add_client(new_socket, tid);

            } // Passe au client suivant
            else{
                perror("Erreur lors de la création du thread client\n");
                close(*client_sock);
                free(client_sock);
                continue;
            }


                //pthread_detach(tid); // Automatically free thread resources
                //free(client_sock);

            //pthread_detach(tid); // Automatically free thread resources
        }   // osf on use join
        else{
            printf("on cancel le serv fd\n");
            break;
        }

    }
    printf("shutdown stp \n");
    shutdown_server();
    //for (int i = 0; i < liste_client.client_count; i++) {
      //  pthread_join(liste_client.client_threads[i], NULL);
    //}


}


int main(void) {
    liste_client.client_count = 0;

    pthread_t signal_tid;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        // bloque les signaux dans les threads clients et principaux
        perror("Erreur lors du blocage des signaux");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&signal_tid, NULL, gestionnaire_sigint_thread, &set) != 0) {
        perror("Erreur lors de la création du thread gestionnaire de signaux");
        exit(EXIT_FAILURE);
    }
    // crée un thread spécifique qui gère le signal

    signal(SIGPIPE, SIG_IGN);
    sock_creation();
    pthread_join(signal_tid, NULL);

    return 0;

}
