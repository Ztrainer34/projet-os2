#include <arpa/inet.h>
#include <sys/socket.h>
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
#include <pthread.h>
#define MAX_PSEUDO_LENGTH 30
size_t OFFSET = 0; //tracks how much space data occupies in shared memory
sig_atomic_t trigger_sigint = 0; // tracks if ctrl+C was clicked
sig_atomic_t PIPE_OK = 0; // tracks if pipes are open
sig_atomic_t is_manual = 0; // tracks if manuel mode activated
sig_atomic_t trigger_cleanup = 0;
struct sockaddr_in server_address;
pthread_mutex_t storing_messages_mutex;

typedef struct{
   int socket_fd;
   char* the_shared_memory;
   int is_bot_flag;
} ThreadArgs;



/**
 * Function to validate a username (pseudo).
 * Ensures it adheres to character and length constraints.
 *
 * @param pseudo The username to validate.
 * @return 0 if valid, error codes otherwise.
 */
int validate_pseudo(const char *pseudo){
   if (strlen(pseudo) > MAX_PSEUDO_LENGTH){
      fprintf(stderr,"Pseudo length exceeded max length\n");
      exit(2);
   }
   if (strpbrk(pseudo,"/-[]")){
      fprintf(stderr,"invalid character\n");
      exit(3);
   }
   if (strcmp(pseudo, ".") == 0 || strcmp(pseudo, "..") == 0){
      fprintf(stderr,"invalid character\n");
      exit(3) ;
   }

   return 0;
}


void check_success(int success){
   if(success <= 0 ){
      if(success == 0){
         fprintf(stderr, "Not in presentation format");
      }
      else{
         perror("inet_pton");
         exit(EXIT_FAILURE);
      }
   }
}


int create_socket(){
   int client_fd;
   //initialise socket
   if((client_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
      perror("socket failed");
      exit(EXIT_FAILURE);
   }

   // initialise the address + port number
   server_address.sin_family = AF_INET;
   server_address.sin_port = htons(1234);
   int success;
   success = inet_pton(AF_INET ,"127.0.0.1", &server_address.sin_addr);
   check_success(success); // checks if inet_pton worked as intended

   char* addr_variable_value ;
   addr_variable_value = getenv("IP_SERVEUR");

   if(addr_variable_value != NULL){ //change the address
      success = inet_pton(AF_INET,addr_variable_value,&server_address.sin_addr);
      check_success(success);
   }

   const char* port_value = getenv("PORT_SERVEUR"); // retrieve the value if the variable exists
   if(port_value != NULL ){ //IP_SERVER is defined
      int value;
      value = atoi(port_value);
      if (value >= 1 && value <= 65535) {
         server_address.sin_port = htons(value);
      }
   }

   return client_fd;
}


void display_messages(char* shared_memory){
    printf("%s",shared_memory);
    fflush(stdout);
    memset(shared_memory, 0, 1024); // Clear 1024 bytes of shared memory
    OFFSET = 0;
}

/**
 * Function to store a message in shared memory.
 * Automatically displays messages if memory reaches its limit.
 *
 * @param shared_memory Pointer to the shared memory created in the main.
 * @param message The message to be stored.
 */

void store_in_memory(char* shared_memory, char* message, char* pseudo_memory) {
    size_t message_size = strlen(message);
    size_t pseudo_destinatair_size = strlen(pseudo_memory);
    size_t total_size = message_size + pseudo_destinatair_size + 5; // +5 pour "[ ]: ", le '\n' et le '\0'

    if (OFFSET + total_size >= 4096) { // Vérifie si la capacité mémoire est dépassée
        display_messages(shared_memory);
    } else {
        // Formate l'entrée combinée comme "[pseudo_destinatair]: message\n"
        snprintf(shared_memory + strlen(shared_memory),
                 4096 - strlen(shared_memory),
                 "[%s] %s\n",
                 pseudo_memory, message);
        //snprintf(shared_memory + OFFSET, SHARED_MEM_SIZE - OFFSET, "[%s] %s\n", pseudo_memory, message);
        OFFSET += total_size;
    }
}


void send_messages(int socket_fd, const char* username, int isbot,int ismanual,char* shared_memory){
    //retrieve message
    char message[1055]; //size sender's username

    while(1){
            // Read user input
            ssize_t n = read(STDIN_FILENO, message, sizeof(message) - 1);
            if (n > 0) {
               message[n - 1] = '\0'; // Remove newline

            } else if (n == -1 && errno == EINTR) {
                // Interrupted by signal, continue
                continue;

            } else if (n == 0) {  // termine le programme en retournant 0
                perror("stdin fermé ou EOF atteint");
                exit(0);

            }
             else if (n < 0) {
                perror("Erreur lors de la lecture");
                exit(EXIT_FAILURE);
            }


            ssize_t success = write(socket_fd, message, strlen(message) + 1);
            if(!isbot){
                printf("[\x1B[4m%s\x1B[0m] %s\n", username, message);
                fflush(stdout);
            }
            if (success == -1){
                    perror("Erreur lors de l'écriture dans le pipe");
                    close(socket_fd);
                    exit(EXIT_FAILURE);
            }
            if (ismanual == 1 && !isbot){
                display_messages(shared_memory);
            }
     }

      close(socket_fd);
}


void* receive_messages(void* args){
   char received_message[1055];

   ThreadArgs* thread_args = (ThreadArgs* ) args;
   int socket_fd = thread_args->socket_fd;
   char* shared_memory = thread_args->the_shared_memory;
   int is_bot = thread_args->is_bot_flag;

   while(1){
      ssize_t n = read(socket_fd, received_message, sizeof(received_message));
      char* sender = strtok(received_message, " "); // split the sender's username with the message
      char* message = strtok(NULL, "");  // the rest of the message
      if (n == 0){
         perror("Entrée standard fermée");
         close(socket_fd);
         exit(0);
      }
      else if (n < 0){
         perror("read");
         close(socket_fd);
         exit(EXIT_FAILURE);
      }

      else if (n > 0){
         //separate sender from message

         if(is_bot && is_manual){ // mode  bot and manual
            printf("\a");
            printf("%s\n",received_message);
            fflush(stdout);

         }

         else if(is_bot && !is_manual){ // mode bot only
               printf("%s\n",received_message);
               fflush(stdout);
         }

         else if(is_manual && !is_bot){ // mode manual only
               printf("\a");
               fflush(stdout);
               store_in_memory(shared_memory,received_message, sender);
         }

      }

   }
   close(socket_fd);
   return NULL;
}



int main(int argc, char* argv[]) {
   //not enough arguments
    if (argc < 2) {
        fprintf(stderr, "chat pseudo_utilisateur pseudo_destinataire [--bot] [--manuel]\n");
        return 1;
   }

   char *username = argv[1];
   validate_pseudo(username);

   int is_bot = 0;
   char* shared_memory = NULL;
   for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--bot") == 0) {
         is_bot = 1;
      }
      else if (strcmp(argv[i], "--manuel") == 0) {
         is_manual = 1;

         //create shared memory
         const int protection = PROT_READ | PROT_WRITE;
         const int visibility = MAP_SHARED | MAP_ANONYMOUS;
         shared_memory = (char*)mmap(NULL, 4096,protection, visibility, -1, 0);

         if(shared_memory == MAP_FAILED ){
               perror("mmap");
               exit(EXIT_FAILURE);
         }
      }
      else{
         fprintf(stderr, "chat pseudo_utilisateur pseudo_destinataire [--bot] [--manuel]\n");
         return -1;
      }
   }

   int client_fd; //create socket
   client_fd = create_socket();


   if(connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
         perror("Connect failed ha");
         close(client_fd);
        return EXIT_FAILURE;
    }

   write(client_fd,username,strlen(username) + 1); // send username to server to identify client

   // create a thread to read from socket
   pthread_t reading_thread;

   ThreadArgs arguments;
   arguments.socket_fd = client_fd;
   arguments.is_bot_flag = is_bot;
   arguments.the_shared_memory = shared_memory;
   pthread_create(&reading_thread, NULL, &receive_messages, (void*)&arguments);

   // Run send_messages in the main thread concurrently
   send_messages(client_fd, username, is_bot, is_manual, shared_memory);

   // Optional: Wait for the reading thread to finish when exiting
   pthread_cancel(reading_thread); // Cancel the reading thread
   pthread_join(reading_thread, NULL);




   return 0;
}
