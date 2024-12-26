#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 10010
#define BUFFER_SIZE 1038 // Maximum size: 2 + 4 + 8 + 1024 bytes

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in server_addr;
    char string_message[1024];
    unsigned char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    struct timeval start, end;
    uint16_t total_length;
    uint32_t sequence_number = 1;  // Initialize sequence number
    uint64_t timestamp;

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    // Prompt user to enter a message
    printf("Enter a message to send (up to 1024 characters): ");
    fgets(string_message, sizeof(string_message), stdin);
    string_message[strcspn(string_message, "\n")] = '\0';  // Remove newline character

    // Get current time for the timestamp
    gettimeofday(&start, NULL);
    timestamp = (uint64_t)(start.tv_sec) * 1000 + (start.tv_usec / 1000);  // Timestamp in milliseconds

    // Construct the message to send (total message length, sequence number, timestamp, and string)
    total_length = 14 + strlen(string_message);  // 2 + 4 + 8 + length of the string

    // Convert fields to network byte order
    total_length = htons(total_length);
    
    // Wrap sequence number at 2^32-1 (4294967295)
    sequence_number = (sequence_number == 4294967295) ? 1 : sequence_number + 1;
    sequence_number = htonl(sequence_number);
    timestamp = htobe64(timestamp);  // Special function to convert 64-bit integer to network byte order

    // Pack the buffer
    memcpy(buffer, &total_length, 2);           // 2 bytes for total length
    memcpy(buffer + 2, &sequence_number, 4);    // 4 bytes for sequence number
    memcpy(buffer + 6, &timestamp, 8);          // 8 bytes for timestamp
    memcpy(buffer + 14, string_message, strlen(string_message));  // Variable-length string

    // Send the message to the server
    if (sendto(sockfd, buffer, 14 + strlen(string_message), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
        perror("sendto failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Receive the response from the server
    ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
    if (bytes_received < 0) {
        perror("recvfrom failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Record the time after receiving
    gettimeofday(&end, NULL);

    // Calculate round-trip time (RTT) in milliseconds
    long rtt = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;

    // Null-terminate the received string and print the response
    buffer[bytes_received] = '\0';
    printf("Received from server: %s\n", buffer + 14);  // Skipping the first 14 bytes (headers)
    printf("Round-trip time: %ld ms\n", rtt);

    close(sockfd);
    return 0;
}
