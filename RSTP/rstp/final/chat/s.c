#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MAX_LINE 256
#define BROADCAST_IP "192.168.0.255"
#define UDP_PORT_TX 12345
#define UDP_PORT_RX 1234
#define MESSAGE "Broadcast message from the A Device"

/* Thread function to monitor port states */
void *monitor_ports(void *arg) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[MAX_LINE];
    char ack_message[] = "ACK";  // Acknowledgment message

    /* Create UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT_RX);
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all interfaces

    /* Bind socket to the address and port */
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        pthread_exit(NULL);
    }

    printf("\n\tServer listening on port %d\n\n", UDP_PORT_RX);

    /* Loop to receive and process messages */
    while (1) {
        int len = recvfrom(sockfd, buffer, MAX_LINE, 0, (struct sockaddr*)&client_addr, &client_len);
        if (len < 0) {
            perror("Recvfrom failed");
            continue;  // Keep the server running
        }

        buffer[len] = '\0';  // Null terminate the received message
        
	        printf("Received message: %s\n", buffer);

         /* Send ACK back to client */
        if (sendto(sockfd, ack_message, strlen(ack_message), 0, (struct sockaddr *)&client_addr, client_len) < 0) {
            perror("Send ACK failed");
        } else {
            printf("Sent ACK to client\n");
        }
       
	printf("-------------------------------------------------------------------\n\n");
    }

    close(sockfd);
    pthread_exit(NULL);
}

int main() {
    pthread_t monitor_thread;
    int sockfd;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;

    /* Create a thread to monitor ports */
    if (pthread_create(&monitor_thread, NULL, monitor_ports, NULL) != 0) {
        perror("Failed to create monitor thread");
        return EXIT_FAILURE;
    }

    /* Create UDP socket for broadcasting */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Enable broadcast option */
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        perror("Broadcast enable failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Configure broadcast address */
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(UDP_PORT_TX);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    /* Send broadcast message every second */
    while (1) {
        if (sendto(sockfd, MESSAGE, strlen(MESSAGE), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("Broadcast failed");
        } else {
           // printf("Sent: %s\n", MESSAGE);
        }
        sleep(1);
    }

    close(sockfd);
    return 0;
}

