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
static volatile sig_atomic_t sigintRecu = 0;
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
            return false;
        }
    }
    return true;
}

bool add_username(char * buffer){
    pthread_mutex_lock(&clients_mutex);
    if (liste_client.client_count < MAX_CLIENTS){
        if (no_space(buffer)){
            liste_client.client_usernames[liste_client.client_count++] = strdup(buffer); // alloue la memoire
            if (!liste_client.client_usernames[liste_client.client_count - 1]) {
                perror("Allocation mémoire échouée");
                exit(EXIT_FAILURE);

            }
            printClientUsernames();
            pthread_mutex_unlock(&clients_mutex);
            return true;
        }
    }
    else{
        pthread_mutex_unlock(&clients_mutex);
        return false;
    }

}

char* get_username(char *buffer) {
    char *username = malloc(20); // Allouer dynamiquement
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
    free(client_sock); // client_sock est inutile

    char buffer[1024];
    while (1) {
        int bytes_read = read(sock, buffer, sizeof(buffer));
        if (bytes_read > 0) {
            pthread_mutex_lock(&clients_mutex);
            if(!add_username(buffer)){   // ajoute un username dans la liste
                // envoie un msg au client si le buffer n'est pas un username
                char username[20] = get_username(buffer);
                for (int i = 0; i < liste_client.client_count; i++){
                    if (strcmp(username, liste_client.client_usernames[i]) == 0){ // verifie si le username est présent
                        write(liste_client.client_sockets[i], buffer, strlen(buffer)); 
                        // envoie un message a la bonne personne 
                    }
                }
            }

            printf(" %s\n", buffer);
            send(sock, "Message received!", strlen("Message received!"), 0);


            pthread_mutex_unlock(&clients_mutex);
        }
        else if (bytes_read == 0) { // Le client a fermé la connexion
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
	void sock_creation(){
            port = 1234; // Reset to default if out of range
        }
    }
    liste_client.client_count = 0; // initialise à 0

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

	void sock_creation(){

        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            if (client_sock == NULL) {
                perror("malloc failed");
                close(new_socket);
                break;
            }
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
    signal(SIGINT, GestionnaireSigint);
    signal(SIGPIPE, GestionnaireSigpipe);
    sock_creation();
    return 0;

}