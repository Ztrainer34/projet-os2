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
   printf("Before creating socket");
   if((client_fd = socket(AF_INET,SOCK_STREAM,0)) < 0){
      perror("socket failed");
      exit(EXIT_FAILURE);
   }
   printf("After creating socket");

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


int main(int argc, char* argv[]) {
   //not enough arguments
    if (argc < 2) {
        fprintf(stderr, "chat pseudo_utilisateur pseudo_destinataire [--bot] [--manuel]\n");
        return 1;
   }

   char *pseudo_utilisateur = argv[1];
   validate_pseudo(pseudo_utilisateur);
   int client_fd;
   client_fd = create_socket();
   // Connect client to server

   if(connect(client_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connect failed");
        close(client_fd);
        return EXIT_FAILURE;
   }

   close(client_fd);
   return 0;
}
