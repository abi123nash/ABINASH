#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define CMD "brctl showstp br0"
#define MAX_LINE 256
#define PORT1 "8001"  // eno1
#define PORT2 "8002"  // enx30de4b49af5e
#define BROADCAST_IP "192.168.0.255"
#define UDP_PORT 12345
#define MESSAGE "Broadcast message from the A Devices"

typedef struct {
    char port_id[10];
    char state[20];
} PortStatus;

/* Function to get port states */
int get_port_status(PortStatus *status1, PortStatus *status2) {
    FILE *fp;
    char line[MAX_LINE];
    int found1 = 0, found2 = 0;

    fp = popen(CMD, "r");
    if (!fp) {
        perror("popen failed");
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "port id")) {
            char temp_id[10], temp_state[20];
            if (sscanf(line, " port id %s state %s", temp_id, temp_state) == 2) {
                if (strcmp(temp_id, PORT1) == 0) {
                    strcpy(status1->port_id, temp_id);
                    strcpy(status1->state, temp_state);
                    found1 = 1;
                } else if (strcmp(temp_id, PORT2) == 0) {
                    strcpy(status2->port_id, temp_id);
                    strcpy(status2->state, temp_state);
                    found2 = 1;
                }
            }
        }
    }

    pclose(fp);
    return (found1 && found2) ? 0 : -1;
}

/* Thread function to monitor port states */
void *monitor_ports(void *arg) {
    PortStatus prev_status1 = {"", ""}, prev_status2 = {"", ""};
    PortStatus curr_status1, curr_status2;

    if (get_port_status(&prev_status1, &prev_status2) == 0) {
        printf("Initial Port Status:\n");
        printf("Port %s - State: %s\n", prev_status1.port_id, prev_status1.state);
        printf("Port %s - State: %s\n", prev_status2.port_id, prev_status2.state);
    } else {
        printf("Error fetching initial status\n");
        return NULL;
    }

    while (1) {
        sleep(2);
        if (get_port_status(&curr_status1, &curr_status2) == 0) {
            if (strcmp(curr_status1.state, prev_status1.state) != 0 || strcmp(curr_status2.state, prev_status2.state) != 0) {
                printf("\nPort Status Changed:\n");
                printf("Port %s - State: %s\n", curr_status1.port_id, curr_status1.state);
                printf("Port %s - State: %s\n", curr_status2.port_id, curr_status2.state);
                prev_status1 = curr_status1;
                prev_status2 = curr_status2;
            }
        } else {
            printf("Error fetching status\n");
        }
    }
    return NULL;
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
    broadcast_addr.sin_port = htons(UDP_PORT);
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

