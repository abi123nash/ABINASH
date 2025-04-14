#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAX_LINE 256
#define UDP_PORT_RX 12345
#define UDP_PORT_TX 1234
#define BUFFER_SIZE 1024

char sender_ip[INET_ADDRSTRLEN];
uint8_t server_connect = 0;

typedef struct {
    char port_id[10];
    char state[20];
} PortStatus;

/* UDP Client Function */
void udp_client() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    char formatted_message[BUFFER_SIZE];
    char board_name[50], port1_id[10], port1_state[20], port2_id[10], port2_state[20];

    /* Create UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT_RX);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    /* Bind the socket */
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Client is listening...\n");

    /* Receive messages */
    while (1) {
        socklen_t addr_len = sizeof(server_addr);
        int recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("I'm the B device\n");
            printf("Received: %s\n", buffer);

            /* Split the received message into components */
            if (sscanf(buffer, "%s %s Port %s - State: %s Port %s - State: %s", board_name, board_name + strlen(board_name) + 1,
                       port1_id, port1_state, port2_id, port2_state) == 6) {
                /* Format the message into the desired format */
                snprintf(formatted_message, sizeof(formatted_message),
                         "Received message: %s\nPort %s - State: %s\nPort %s - State: %s\n",
                         board_name, port1_id, port1_state, port2_id, port2_state);

                /* Print the formatted message */
                printf("%s", formatted_message);
            } else {
                printf("Error parsing the message\n");
            }

            /* Get the sender's IP address */
            inet_ntop(AF_INET, &server_addr.sin_addr, sender_ip, sizeof(sender_ip));
            printf("Message received from %s:%d\n", sender_ip, UDP_PORT_RX);
            server_connect = 1;
        }
    }

    close(sockfd);
}

int main() {
    /* Run UDP client in the main thread */
    udp_client();

    return 0;
}

