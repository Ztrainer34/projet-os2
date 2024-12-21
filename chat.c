#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stddef.h>
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
#include <semaphore.h>


#define MAX_PSEUDO_LENGTH 30
#define SEM_NAME "/can_display_message"
#define MEM_SIZE 4096

size_t OFFSET = 0; //tracks how much space data occupies in shared memory
sig_atomic_t trigger_sigint = 0; // tracks if ctrl+C was clicked
sig_atomic_t PIPE_OK = 0; // tracks if pipes are open
sig_atomic_t is_manual = 0; // tracks if manuel mode activated
sig_atomic_t trigger_cleanup = 0; 
sig_atomic_t created_a_socket = 0;
struct sockaddr_in server_address; //initialise structure for socket
char* shared_memory = NULL;
/*Structure used to store the arguments passed in the secondary thead
* It contains 3 elements:
* the socket to intercept messages from other clients
* a flag to determine if bot mode was activated
* a semaphore to ensure mutual exclive access to the console*/
typedef struct{
   int socket_fd;
   int is_bot_flag;
   sem_t* semaphore;
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


void cleanup(char* memory, sem_t* semaphore,int socket){
   if(semaphore!= NULL) sem_destroy(semaphore);
   if(socket >= 0) close(socket);
   if(memory != NULL){
      free(memory);
      memory = NULL;
   }
   
}


static void sigint_handler(int signal){
   if(signal == SIGINT){
      if(created_a_socket && is_manual){
         trigger_sigint = 1; // the messages in memory shall be printed out
      }
      else if(is_manual && !created_a_socket){
         free(shared_memory);
         shared_memory = NULL;
         exit(4);
      }
      else{
         exit(4);
      }
   }
}



void check_address(int success){
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

   created_a_socket = 1;

   // initialise the address + port number
   server_address.sin_family = AF_INET;
   server_address.sin_port = htons(1234);
   int success;
   success = inet_pton(AF_INET ,"127.0.0.1", &server_address.sin_addr);
   check_address(success); // checks if inet_pton worked as intended

   char* addr_variable_value ;
   addr_variable_value = getenv("IP_SERVEUR");
   
   if(addr_variable_value != NULL){ //change the address 
      success = inet_pton(AF_INET,addr_variable_value,&server_address.sin_addr);
      check_address(success);
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


/**
 * Function to display the messages stored in the shared memory.
 * Clears the memory after displaying.
 *
 * @param shared_memory Pointer to the shared memory created in the main.
 */
void display_messages(char* shared_memory){
    printf("%s",shared_memory);
    fflush(stdout);
    memset(shared_memory, 0, 4096);
    OFFSET = 0;
}


/**
 * Function to store a message in shared memory.
 * Automatically displays messages if memory reaches its limit.
 *
 * @param shared_memory Pointer to the shared memory created in the main.
 * @param message The message to be stored.
 */

void store_in_memory(char* shared_memory, char* message, char* pseudo_memory,sem_t* semaphore) {
    size_t message_size = strlen(message);
    size_t pseudo_destinatair_size = strlen(pseudo_memory);
    size_t total_size = message_size + pseudo_destinatair_size + 5; // +5 pour "[ ]: ", le '\n' et le '\0'

    if (OFFSET + total_size >= 4096) { // Vérifie si la capacité mémoire est dépassée
       sem_wait(semaphore);
       display_messages(shared_memory);
       sem_post(semaphore);
    } else {
        // Formate l'entrée combinée comme "[pseudo_destinatair]: message\n"
        snprintf(shared_memory + OFFSET,
                 4096 - OFFSET,
                 "[%s] %s\n",
                 pseudo_memory, message);
        //snprintf(shared_memory + OFFSET, SHARED_MEM_SIZE - OFFSET, "[%s] %s\n", pseudo_memory, message);
        OFFSET += total_size;
       
    }
}


void send_messages(int socket_fd, const char* username, int isbot,sem_t* semaphore){
    //retrieve message
    char message[1056]; //size sender's username

    while(1){
            if(trigger_sigint && is_manual){
               sem_wait(semaphore);
               display_messages(shared_memory);
               sem_post(semaphore);
               trigger_sigint = 0; // reset the flag to 0
            }
            // Read user input
            ssize_t n = read(STDIN_FILENO, message, sizeof(message) - 1);
            if (n > 0) {
               message[n - 1] = '\n'; // Remove newline

            } else if (n == -1 && errno == EINTR) {
                // Interrupted by signal, continue
                continue;

            } else if (n == 0) {  // termine le programme en retournant 0
               perror("stdin fermé ou EOF atteint");
               cleanup(shared_memory,semaphore,socket_fd);
               exit(0);

            }
             else if (n < 0) {
                perror("Erreur lors de la lecture");
                cleanup(shared_memory,semaphore,socket_fd);
                exit(EXIT_FAILURE);
            }


            ssize_t success = send(socket_fd, message, strlen(message),MSG_DONTWAIT);
            if (success == -1){
                    perror("Erreur lors de l'écriture dans le pipe");
                    cleanup(shared_memory,semaphore,socket_fd);
                    exit(EXIT_FAILURE);
            } 


            if(!isbot ){
               strtok(message, " ");
               char* message = strtok(NULL, "");
               sem_wait(semaphore);
               printf("[\x1B[4m%s\x1B[0m] %s\n", username, message);
               fflush(stdout);
               sem_post(semaphore);
            }
            if (is_manual == 1 && !isbot && OFFSET > 0){
               sem_wait(semaphore);
               display_messages(shared_memory);
               sem_post(semaphore);
            }
     }

      close(socket_fd);
}


void* receive_messages(void* args){
   char received_message[1056]; //size of sender's name ,message and the space between them

   ThreadArgs* thread_args = (ThreadArgs* ) args;
   int socket_fd = thread_args->socket_fd;
   int is_bot = thread_args->is_bot_flag;
   sem_t* print_semaphore = thread_args->semaphore;

   while(1){
      ssize_t n = read(socket_fd, received_message, sizeof(received_message));
      if (n == 0){
         perror("Entrée standard fermée");
         cleanup(shared_memory,print_semaphore,socket_fd);
         exit(0);
      }
      else if (n < 0){
         perror("read");
         cleanup(shared_memory,print_semaphore,socket_fd);
         exit(EXIT_FAILURE);
      }

      else{
         //separate sender from message
         char* sender = strtok(received_message, " "); // split the sender's username with the message
         char* message = strtok(NULL, "");

         if(is_bot && is_manual){        // mode  bot and manual 
               sem_wait(print_semaphore);
               printf("\a");
               printf("[%s] %s\n",sender,message);
               fflush(stdout);
               sem_post(print_semaphore);
                
            }
            
            else if(is_bot && !is_manual){ // mode bot only
                sem_wait(print_semaphore);
                printf("[%s] %s\n",sender,message);
                fflush(stdout);
                sem_post(print_semaphore);
            }
            else if(!is_manual && !is_bot){ // without mode
               sem_wait(print_semaphore);
               printf("[\x1B[4m%s\x1B[0m] %s\n",sender,message);
               fflush(stdout);
               sem_post(print_semaphore);
            }

            else if(is_manual && !is_bot){ // mode manual only
               sem_wait(print_semaphore);
                printf("\a");
                fflush(stdout);
               sem_post(print_semaphore);
                store_in_memory(shared_memory,message, sender,print_semaphore);
            }
            
      }

   }
   close(socket_fd);
   return NULL;
}



int main(int argc, char* argv[]) {
   //not enough or too many arguments 
    if (argc < 2 && argc > 4) {
        fprintf(stderr, "chat pseudo_utilisateur [--bot] [--manuel]\n");
        return 1;
   }

   char *username = argv[1];
   validate_pseudo(username);


   int is_bot = 0;

   for (int i = 2; i < argc; i++) {
      if (strcmp(argv[i], "--bot") == 0) {
         is_bot = 1;
      }
      else if (strcmp(argv[i], "--manuel") == 0) {
         is_manual = 1;

         //create shared memory
         if((shared_memory = (char*) malloc(MEM_SIZE)) == NULL){ //allocate memory to store messages
            perror("malloc");
            return -1;
         }
      }
      else{
         fprintf(stderr, "chat pseudo_utilisateur[--bot] [--manuel]\n");
         return -1;
      }
   }

   struct sigaction sa; 
   sa.sa_handler = sigint_handler;
   sa.sa_flags = 0;
   sigemptyset(&sa.sa_mask);
   if (sigaction(SIGINT, &sa, NULL) == -1) {
      free(shared_memory);
      shared_memory = NULL;
      perror("Erreur lors de l'initialisation du gestionnaire SIGINT");
      exit(EXIT_FAILURE);
   }
   
   int client_fd; //create socket 
   client_fd = create_socket();

   // Connect client to server
   if(connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        cleanup(shared_memory,NULL,client_fd);
        perror("Connect failed");
        close(client_fd);
        return EXIT_FAILURE;
   }

   sem_t* print_semaphore = sem_open(SEM_NAME,O_CREAT,0666,1); //create + open semaphore

   ssize_t sent_bytes = send(client_fd,username,strlen(username),0);// send username to server to identify client
   if(sent_bytes == -1){ //error handling
      perror("send failed");
      cleanup(shared_memory,print_semaphore,client_fd);
      return -1;
   }

   // create a thread to read from socket
   pthread_t reading_thread;
   ThreadArgs arguments;
   arguments.socket_fd = client_fd;
   arguments.is_bot_flag = is_bot;
   arguments.semaphore = print_semaphore;
   pthread_create(&reading_thread,NULL,&receive_messages,(void*)&arguments);  

   send_messages(client_fd,username,is_bot,print_semaphore);
   

   pthread_join(reading_thread,NULL); // wait for the thread to terminate   

   return 0;
}
