#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <semaphore.h>
#include <endian.h>

#define PORT 10010
#define BUFFER_SIZE 1038  // Maximum size: 2 + 4 + 8 + 1024 bytes
#define MAX_THREADS 10    // Maximum number of concurrent threads

sem_t thread_semaphore;  // Semaphore to limit the number of threads

void *handle_client(void *client_socket) {
    int sockfd = *((int *)client_socket);
    free(client_socket);

    struct sockaddr_in client_addr;
    unsigned char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);
    uint16_t total_length;
    uint32_t sequence_number;
    uint64_t timestamp;

    while (1) {
        // Receive message from client
        ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        if (bytes_received < 0) {
            perror("recvfrom failed");
            break;
        }

        // Unpack the received message
        memcpy(&total_length, buffer, 2);        // 2 bytes for total message length
        memcpy(&sequence_number, buffer + 2, 4); // 4 bytes for sequence number
        memcpy(&timestamp, buffer + 6, 8);       // 8 bytes for timestamp

        // Convert fields from network byte order to host byte order
        total_length = ntohs(total_length);
        sequence_number = ntohl(sequence_number);
        timestamp = be64toh(timestamp);

        // Null-terminate the string part and print the extracted data
        buffer[bytes_received] = '\0';  // Null terminate the string
        printf("Received from client - Sequence Number: %u, Timestamp: %lu, Message: %s\n", sequence_number, timestamp, buffer + 14);

        // Echo the exact message back to the client
        if (sendto(sockfd, buffer, bytes_received, 0, (struct sockaddr *)&client_addr, addr_len) < 0) {
            perror("sendto failed");
            break;
        }

        printf("Server echoed message to client: %s\n", buffer + 14);

        // Check for termination signal "END" but don't shut down the server
        if (strcmp((char *)buffer + 14, "END") == 0) {
            printf("Received 'END' from client but continuing to listen...\n");
        }
    }

    sem_post(&thread_semaphore);  // Release semaphore
    return NULL;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;

    // Initialize semaphore
    sem_init(&thread_semaphore, 0, MAX_THREADS);

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR option to allow immediate reuse of the port
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Bind to local loopback

    // Bind the socket to the port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is running on port %d...\n", PORT);

    while (1) {
        // Wait for an available thread slot
        sem_wait(&thread_semaphore);

        int *client_socket = malloc(sizeof(int));
        *client_socket = sockfd;

        // Create a new thread for each client
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("pthread_create failed");
            free(client_socket);
            sem_post(&thread_semaphore);  // Release semaphore if thread creation fails
        }

        // Detach the thread
        pthread_detach(thread_id);
    }

    close(sockfd);
    sem_destroy(&thread_semaphore);  // Clean up semaphore
    printf("Server shut down successfully.\n");
    return 0;
}
