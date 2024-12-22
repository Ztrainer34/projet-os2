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

// Global variables for signal handling and client management
static volatile sig_atomic_t sigint_recu = 0;
static volatile sig_atomic_t triggerCleanup = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for synchronising
pthread_mutex_t sigint_mutex = PTHREAD_MUTEX_INITIALIZER;

// A mutex is used to prevent simultaneous access by threads to sigint_recu

int server_fd;

// Structure to manage connected clients
struct ListeClient {
    int client_sockets[MAX_CLIENTS];  // Vector to store client sockets
    int client_count;             // Numbers of client connected
    pthread_t client_threads[MAX_CLIENTS];
    char* client_usernames[MAX_CLIENTS];
};

struct ListeClient liste_client;

#include <unistd.h> // read()
#include <stdio.h>  // perror()
#include <errno.h>  // errno




/**
 * Signal handler thread for SIGINT.
 * Waits for the SIGINT signal and sets a flag to shut down the server.
 */
void* gestionnaire_sigint_thread(void* set_){
    int sig;
    sigset_t *set = (sigset_t *)set_;

    if (sigwait(set, &sig) != 0) { // waiting for the signal
        perror("Error while waiting for the signal");

    }

    if (sig == SIGINT) {// use `shutdown` to interrompt `accept`


        if (shutdown(server_fd, SHUT_RDWR) < 0) {
            perror("Error while shutting down the server");
        }

        pthread_mutex_lock(&sigint_mutex);
        sigint_recu = 1; // Déclenche la fermeture
        pthread_mutex_unlock(&sigint_mutex);
    }

    return NULL;
}


/**
 * Shuts down the server by closing all client connections and cleaning up resources.
 */

void shutdown_server(){
    printf(" serveur closed\n ");
    pthread_mutex_lock(&clients_mutex);
    const char* server_shutdown = " server disconected. \n";
    for (int i = 0; i < liste_client.client_count; i++) {
        send(liste_client.client_sockets[i], server_shutdown, strlen(server_shutdown), 0);
        // envoie un msg a chaque client que le serveur se déconnecte
        if(liste_client.client_usernames[i]){
            free(liste_client.client_usernames[i]);
            liste_client.client_usernames[i] = NULL; // Évitez une double libération
        }
        pthread_detach(liste_client.client_threads[i]);

        close(liste_client.client_sockets[i]);

     }



    close(server_fd);
    liste_client.client_count = 0;
    pthread_mutex_unlock(&clients_mutex);
    pthread_mutex_destroy(&clients_mutex); // libere la memoire du mutex
    exit(EXIT_SUCCESS);
}


/**
 * Adds a client to the client list.
 *
 * @param client_sock Socket descriptor of the client.
 * @param tid Thread ID handling the client.
 */
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


/**
 * Checks if a buffer contains spaces.
 *
 * @param buffer Input buffer.
 * @return true if no spaces are found, false otherwise.
 */
bool no_space(const char * buffer){
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] == ' '){

            return false;
        }
    }

    return true;
}

/**
 * Adds a username to the client list.
 *
 * @param buffer The username string.
 */

void add_username(char * buffer) {
    pthread_mutex_lock(&clients_mutex);
    int idx = liste_client.client_count - 1; // Dernier client ajouté
    if (liste_client.client_count < MAX_CLIENTS && liste_client.client_usernames[idx] == NULL) {
        if (no_space(buffer)) {
            liste_client.client_usernames[idx] = strdup(buffer); // Fixe un bug : utilisation de strdup
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}


/**
 * Handles client communication in a separate thread.
 *
 * @param client_sock Pointer to the client's socket descriptor.
 * @return NULL
 */


int SafeRead(int fd, char* buffer, size_t size) {
    int total_read = 0, ret;

    // Continue reading until no more data is available (EOF)
    while (true) {
        ret = read(fd, buffer + total_read, size - total_read);

        if (ret > 0) {
            total_read += ret;


            break;
        } else if (ret == 0) {  // End of file reached

            break;
        } else {  // Error or interrupted read
            if (errno == EINTR) {

                continue;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {

                break;
            } else {

                return -1; // Indicate error
            }
        }
    }


    return total_read; // Return total bytes read
}



void *handle_client(void *client_sock) {
    int sock = *(int *)client_sock;
    free(client_sock); // client_sock est inutile à présent libere
    char buffer[1054];
    while (!sigint_recu) {

        ssize_t bytes_read = SafeRead(sock, buffer, sizeof(buffer));
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

                    ssize_t sent_bytes = send(recipient_sock, formatted_message, strlen(formatted_message), 0);
                    if (sent_bytes == -1) {
                        perror("Error sending message to recipient");
                    } else {
                        printf("Message sent to %s : %s\n", recipient, message);
                    }
                }
                else {
                    // Notify sender that the recipient was not found
                    char error_msg[64];
                    snprintf(error_msg, sizeof(error_msg), "this personne (%s) is not connected.\n", recipient);
                    //const char *err_msg = "Recipient not found\n"; // remplacer par cette prsn pas co
                    // envoie a l'utilisateur le msg d'erreur
                    ssize_t send_bytes = send(sock, error_msg, strlen(error_msg), 0);
                    if (send_bytes == -1) {
                        perror("Error while sending the error message to the sender");
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
            perror("Error while reading\n");
            // faire fonction cleanup
        }

    }


    //close(sock);
    return NULL;
}


/**
 * Creates the server socket, binds it, and listens for incoming connections.
 */
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
    // Permet la réutilisation du port/de l'adresse" translates to "Allows port/address reuse
    if (setsockopt (server_fd , SOL_SOCKET , SO_REUSEADDR | SO_REUSEPORT , &opt , sizeof (opt )) < 0){
        perror("setsock");
    }
    struct sockaddr_in address ;
    address . sin_family = AF_INET ;
    address . sin_addr .s_addr = INADDR_ANY ;
    address . sin_port = htons (port);

    // Defines the address and the listening port, reserves the port
    checked(bind(server_fd , ( struct sockaddr *)& address , sizeof ( address )));
    // start listening
    checked(listen (server_fd , 5)); // Maximum 3 pending connections
    size_t addrlen = sizeof ( address );
    // Opens a new connection
    while (!sigint_recu) {
        int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (new_socket >= 0) {
            int *client_sock = (int*)malloc(sizeof(int));
            if (client_sock == NULL) {
                perror("malloc failed");
                close(new_socket);
                continue; // go to next client
            }

            *client_sock = new_socket;

            pthread_t tid;

            if (pthread_create(&tid, NULL, handle_client, client_sock)== 0) {
                add_client(new_socket, tid);

            } // go to next client
            else{
                perror("Error while creating the client thread\n");
                close(*client_sock);
                free(client_sock);
                continue;
            }


                //pthread_detach(tid); // Automatically free thread resources
                //free(client_sock);

            //pthread_detach(tid); // Automatically free thread resources
        }   // osf on use join
        else{

            break;
        }

    }

    shutdown_server();
    //for (int i = 0; i < liste_client.client_count; i++) {
      //  pthread_join(liste_client.client_threads[i], NULL);
    //}


}

/**
 * Main function initializes signal handling and starts the server.
 */
int main(void) {
    liste_client.client_count = 0;

    pthread_t signal_tid;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
        // Blocks signals in the client and main threads
        perror("Error while blocking signals");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&signal_tid, NULL, gestionnaire_sigint_thread, &set) != 0) {
        perror("Error while creating the signal handler thread");
        exit(EXIT_FAILURE);
    }
    // Creates a specific thread that handles the signal

    signal(SIGPIPE, SIG_IGN);
    sock_creation();
    pthread_join(signal_tid, NULL);

    return 0;

}
