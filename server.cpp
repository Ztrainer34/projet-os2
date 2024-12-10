#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
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
#define MAX_PSEUDO_LENGTH 30
size_t OFFSET = 0; //tracks how much space data occupies in shared memory
sig_atomic_t trigger_sigint = 0; // tracks if ctrl+C was clicked
sig_atomic_t PIPE_OK = 0; // tracks if pipes are open
sig_atomic_t is_manual = 0; // tracks if manuel mode activated
sig_atomic_t trigger_cleanup = 0; 
struct sockaddr_in server_address;



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


void* send_messages(int socket_fd, const char* username, int isbot,int ismanual,char* shared_memory){
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

   // Connect client to server
   if(connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connect failed");
        close(client_fd);
        return EXIT_FAILURE;
   }
   send_messages(client_fd,username,is_bot,is_manual,shared_memory);
   // create a thread to read from socket
   return 0;
}
