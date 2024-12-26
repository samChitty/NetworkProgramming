#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits.h>


#define PORT 10020
#define REQUEST_SIZE 9   // Size of the request message
#define RESPONSE_SIZE 14  // Size of the response message


void handle_client(int client_sock) {
   unsigned char request[REQUEST_SIZE];
   unsigned char response[RESPONSE_SIZE];


   // Receive the request message
   ssize_t bytes_received = recv(client_sock, request, REQUEST_SIZE, 0);
   if (bytes_received < 0) {
       perror("recv failed");
       close(client_sock);
       return;
   }


   // Extract the operator and operands
   char operator = request[0];               // Operator (1 byte)
   unsigned int operandA, operandB, result = 0;
   memcpy(&operandA, &request[1], 4);        // Operand A (4 bytes)
   memcpy(&operandB, &request[5], 4);        // Operand B (4 bytes)


   unsigned char is_valid = 1;               // Validity of the answer


   // Perform the operation with overflow detection
   switch (operator) {
       case '+':
           if (operandA > (UINT_MAX - operandB)) {
               result = 0;  // Overflow occurred
               is_valid = 2;  // Set answer as invalid
           } else {
               result = operandA + operandB;
           }
           break;
       case '-':
           result = operandA - operandB;
           break;
       case 'x':
           if (operandA > 0 && operandB > 0 && operandA > (UINT_MAX / operandB)) {
               result = 0;  // Overflow occurred
               is_valid = 2;  // Set answer as invalid
           } else {
               result = operandA * operandB;
           }
           break;
       case '/':
           if (operandB == 0) {
               result = 0;  // Set result to 0 in case of invalid operation
               is_valid = 2;  // Invalid result
           } else {
               result = operandA / operandB;
           }
           break;
       default:
           fprintf(stderr, "Unknown operator\n");
           close(client_sock);
           return;
   }


   // Create the response message
   response[0] = operator;                   // 1-byte operator
   memcpy(&response[1], &operandA, 4);       // 4-byte operand A
   memcpy(&response[5], &operandB, 4);       // 4-byte operand B
   memcpy(&response[9], &result, 4);         // 4-byte result
   response[13] = is_valid;                  // 1-byte validity indicator


   // Send the response message
   if (send(client_sock, response, RESPONSE_SIZE, 0) < 0) {
       perror("send failed");
   }


   close(client_sock);  // Close the client socket after handling
}


int main() {
   int sockfd, client_sock;
   struct sockaddr_in server_addr, client_addr;
   socklen_t client_addr_len = sizeof(client_addr);


   // Create TCP socket
   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
       perror("socket failed");
       exit(EXIT_FAILURE);
   }


   // Configure server address
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(PORT);
   server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Localhost


   // Bind the socket to the port
   if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
       perror("bind failed");
       close(sockfd);
       exit(EXIT_FAILURE);
   }


   // Listen for incoming connections
   if (listen(sockfd, 5) < 0) {
       perror("listen failed");
       close(sockfd);
       exit(EXIT_FAILURE);
   }


   printf("Server is running on port %d...\n", PORT);


   while (1) {
       // Accept a new connection
       client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
       if (client_sock < 0) {
           perror("accept failed");
           continue;
       }


       // Handle the client request
       handle_client(client_sock);
   }


   close(sockfd);  // Close the server socket
   return 0;
}
