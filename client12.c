#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <limits.h>


#define PORT 10020
#define BUFFER_SIZE 14  // Size of the response message
#define REQUEST_SIZE 9   // Size of the request message


int main(int argc, char *argv[]) {
   if (argc != 4) {
       fprintf(stderr, "Usage: %s <operandA> <operandB> <operator>\n", argv[0]);
       exit(EXIT_FAILURE);
   }


   // Parse command-line arguments
   long long int operandA = atoll(argv[1]);
   long long int operandB = atoll(argv[2]);
   char operator = argv[3][0];


   // Ensure operands are positive unsigned 32-bit integers
   if (operandA < 0 || operandB < 0 || operandA > UINT_MAX || operandB > UINT_MAX) {
       fprintf(stderr, "Invalid input: operands must be non-negative unsigned integers.\n");
       exit(EXIT_FAILURE);
   }


   // Ensure operator is valid
   if (operator != '+' && operator != '-' && operator != 'x' && operator != '/') {
       fprintf(stderr, "Invalid operator. Use +, -, x, or /\n");
       exit(EXIT_FAILURE);
   }


   // Create the request message (9 bytes)
   unsigned char request[REQUEST_SIZE];
   request[0] = operator;  // 1-byte operator
   unsigned int opA = (unsigned int) operandA;
   unsigned int opB = (unsigned int) operandB;
   memcpy(&request[1], &opA, 4);  // 4-byte operand A (in network byte order)
   memcpy(&request[5], &opB, 4);  // 4-byte operand B (in network byte order)


   // Create TCP socket
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0) {
       perror("socket failed");
       exit(EXIT_FAILURE);
   }


   // Configure server address
   struct sockaddr_in server_addr;
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(PORT);
   inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);  // Localhost


   // Connect to the server
   if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
       perror("connect failed");
       close(sockfd);
       exit(EXIT_FAILURE);
   }


   // Send the request message
   if (send(sockfd, request, REQUEST_SIZE, 0) < 0) {
       perror("send failed");
       close(sockfd);
       exit(EXIT_FAILURE);
   }


   // Receive the response message
   unsigned char response[BUFFER_SIZE];
   ssize_t bytes_received = recv(sockfd, response, BUFFER_SIZE, 0);
   if (bytes_received < 0) {
       perror("recv failed");
       close(sockfd);
       exit(EXIT_FAILURE);
   }


   // Extract values from the response message
   unsigned int operandA_resp, operandB_resp, result;
   memcpy(&operandA_resp, &response[1], 4);  // Operand A from server
   memcpy(&operandB_resp, &response[5], 4);  // Operand B from server
   memcpy(&result, &response[9], 4);         // Result from server
   unsigned char is_valid = response[13];    // Is Answer Valid (1 byte)


   // Print the result
   if (is_valid == 1) {
       printf("Result: %u %c %u = %u\n", operandA_resp, operator, operandB_resp, result);
   } else {
       printf("Error: Invalid operation (e.g., overflow or division by zero)\n");
   }


   // Close the socket
   close(sockfd);
   return 0;
}