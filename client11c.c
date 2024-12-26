#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/wait.h>

#define PORT 10010
#define BUFFER_SIZE 1038  // 2 + 4 + 8 + 1024 (max string length)
#define NUM_MESSAGES 10000

// Sender process: Sends numbers to the server
void sender(const char *server_ip, int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len) {
    char buffer[BUFFER_SIZE];
    uint16_t total_length;
    uint32_t sequence_number = 1;
    uint64_t timestamp;
    struct timeval start;

    for (int i = 1; i <= NUM_MESSAGES; i++) {
        // Get current time for the timestamp
        gettimeofday(&start, NULL);
        timestamp = (uint64_t)(start.tv_sec) * 1000 + (start.tv_usec / 1000);  // Timestamp in milliseconds

        // Wrap sequence number at 2^32-1 (4294967295)
        sequence_number = (sequence_number == 4294967295) ? 1 : sequence_number + 1;

        // Convert fields to network byte order
        sequence_number = htonl(sequence_number);
        timestamp = htobe64(timestamp);

        // Create the message to send
        snprintf(buffer + 14, BUFFER_SIZE - 14, "%d", i);  // Place the message after the first 14 bytes

        total_length = htons(14 + strlen(buffer + 14));  // Message length = 2 (length) + 4 (sequence) + 8 (timestamp) + message length
        memcpy(buffer, &total_length, 2);               // 2 bytes for total message length
        memcpy(buffer + 2, &sequence_number, 4);        // 4 bytes for sequence number
        memcpy(buffer + 6, &timestamp, 8);              // 8 bytes for timestamp

        // Send the message to the server
        if (sendto(sockfd, buffer, ntohs(total_length), 0, (struct sockaddr *)server_addr, addr_len) < 0) {
            perror("sendto failed");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        usleep(1000);  // Sleep for 1 millisecond between sends
    }

    // Send termination signal
    snprintf(buffer + 14, BUFFER_SIZE - 14, "END");
    total_length = htons(14 + strlen(buffer + 14));
    memcpy(buffer, &total_length, 2);               // 2 bytes for total message length
    memcpy(buffer + 2, &sequence_number, 4);        // 4 bytes for sequence number
    memcpy(buffer + 6, &timestamp, 8);              // 8 bytes for timestamp

    sendto(sockfd, buffer, ntohs(total_length), 0, (struct sockaddr *)server_addr, addr_len);
    printf("Sender finished sending messages\n");
}

// Receiver process: Receives messages from the server and prints statistics
void receiver(int sockfd) {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];
    int missing[NUM_MESSAGES] = {0};
    int received_count = 0;
    long min_rtt = 100000, max_rtt = 0, total_rtt = 0;

    struct timeval start, end;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &(struct timeval){20, 0}, sizeof(struct timeval));

    while (received_count < NUM_MESSAGES) {
        gettimeofday(&start, NULL);

        ssize_t bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (bytes_received < 0) {
            perror("recvfrom failed or timed out");
            break;
        }

        buffer[bytes_received] = '\0';

        // Extract message length, sequence number, and timestamp
        uint16_t total_length;
        uint32_t sequence_number;
        uint64_t timestamp;
        memcpy(&total_length, buffer, 2);
        memcpy(&sequence_number, buffer + 2, 4);
        memcpy(&timestamp, buffer + 6, 8);

        // Convert fields from network byte order to host byte order
        total_length = ntohs(total_length);
        sequence_number = ntohl(sequence_number);
        timestamp = be64toh(timestamp);

        // Check if the received message contains the "END" signal
        if (strcmp(buffer + 14, "END") == 0) {
            break;
        }

        int received_number = atoi(buffer + 14);  // Extract the number part from the message

        if (received_number >= 1 && received_number <= NUM_MESSAGES) {
            missing[received_number - 1] = 1;
            received_count++;

            gettimeofday(&end, NULL);
            long rtt = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
            total_rtt += rtt;
            if (rtt < min_rtt) min_rtt = rtt;
            if (rtt > max_rtt) max_rtt = rtt;
        }
    }

    // Summary report
    int missing_count = 0;
    for (int i = 0; i < NUM_MESSAGES; i++) {
        if (missing[i] == 0) {
            missing_count++;
        }
    }

    printf("Summary Report:\n");
    printf("Total messages received: %d\n", received_count);
    printf("Missing messages: %d\n", missing_count);
    if (received_count > 0) {
        printf("Min RTT: %ld ms\n", min_rtt);
        printf("Max RTT: %ld ms\n", max_rtt);
        printf("Average RTT: %ld ms\n", total_rtt / received_count);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        sender(argv[1], sockfd, &server_addr, addr_len);
    } else {
        receiver(sockfd);
        wait(NULL);
    }

    close(sockfd);
    return 0;
}
