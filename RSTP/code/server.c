#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BROADCAST_IP "192.168.0.255"
#define PORT 12345
#define MESSAGE "Broadcast message from the A Devices"

int main() {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    int broadcast_enable = 1;
    
    /* Create UDP socket */
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
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);
    
    /* Send broadcast message every second */
    while (1) {
        if (sendto(sockfd, MESSAGE, strlen(MESSAGE), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("Broadcast failed");
        } else {
            printf("Sent: %s\n", MESSAGE);
        }
        sleep(1);
    }
    
    close(sockfd);
    return 0;
}

