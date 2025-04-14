#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define CMD "brctl showstp br0"
#define BOARD_NAME "Im Board B"
#define MAX_LINE 256
#define PORT1 "8001"  // eno1
#define PORT2 "8002"  // enx30de4b49af5e
#define UDP_PORT_RX 12345
#define UDP_PORT_TX 1234
#define BUFFER_SIZE 1024

char sender_ip[INET_ADDRSTRLEN];
uint8_t server_connect=0;

typedef struct {
    char port_id[10];
    char state[20];
} PortStatus;

/* Function to get port states */
int get_port_status(PortStatus *status1, PortStatus *status2) {
    FILE *fp;
    char line[MAX_LINE];
    int found1 = 0, found2 = 0;

    /* Open command output */
    fp = popen(CMD, "r");
    if (!fp) {
        perror("popen failed");
        return -1;
    }

    /* Read and parse output */
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "port id")) {
            char temp_id[10], temp_state[20];

            /* Extract port ID */
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

/* Function to send port status over UDP */
int send_port_status(const char* sender_ip, const PortStatus* status1, const PortStatus* status2) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    /* Create UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if(server_connect == 0)
    {    
           return 1;
    }	  
	    

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT_TX);
    if (inet_pton(AF_INET, sender_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid sender IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    /* Prepare status message */
    snprintf(buffer, sizeof(buffer), "%s Port %s - State: %s\nPort %s - State: %s\n",BOARD_NAME,
             status1->port_id, status1->state, status2->port_id, status2->state);

    /* Send the message */
    if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Send failed");
    } else {
        printf("Port status sent to %s:%d\n", sender_ip, UDP_PORT_TX);
    }
    //server_connect = 0;
    close(sockfd);
}

/* Monitoring function (runs as a separate thread) */
void* monitor_ports(void* arg) {
    PortStatus prev_status1 = {"", ""}, prev_status2 = {"", ""};
    PortStatus curr_status1, curr_status2;
    int flag;
     
    while (1) {
        sleep(2);  // Monitoring interval

        if (get_port_status(&curr_status1, &curr_status2) == 0) {
            if (strcmp(curr_status1.state, prev_status1.state) != 0 || 
                strcmp(curr_status2.state, prev_status2.state) != 0) {

                printf("\nPort Status Changed:\n");
                printf("Port %s - State: %s\n", curr_status1.port_id, curr_status1.state);
                printf("Port %s - State: %s\n", curr_status2.port_id, curr_status2.state);

                /* Send updated status to the sender IP */
                flag = send_port_status(sender_ip, &curr_status1, &curr_status2);

                /* Update previous status */
                prev_status1 = curr_status1;
                prev_status2 = curr_status2;
            }
        } else {
            printf("Error fetching status\n");
        }

	if(flag == 1)
        {		
         flag=send_port_status(sender_ip, &curr_status1, &curr_status2);
	}
    }

    return NULL;
}

/* UDP Client Function */
void udp_client() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];

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
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP Client is listening...\n");

    /* Receive messages */
    while (1) {
        socklen_t addr_len = sizeof(server_addr);
        int recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&server_addr, &addr_len);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("I'm the B device\n");
            printf("Received: %s\n", buffer);
        
            /* Get the sender's IP address */
            inet_ntop(AF_INET, &server_addr.sin_addr, sender_ip, sizeof(sender_ip));
            printf("Message received from %s:%d\n", sender_ip, UDP_PORT_RX);
	    server_connect=1;
        }
    }

    close(sockfd);
}

int main() {
    pthread_t monitor_thread;

    /* Start monitoring ports in a separate thread */
    if (pthread_create(&monitor_thread, NULL, monitor_ports, NULL) != 0) {
        perror("Failed to create monitoring thread");
        return EXIT_FAILURE;
    }

    /* Run UDP client in the main thread */
    udp_client();

    /* Join thread (this won't actually execute since udp_client runs indefinitely) */
    pthread_join(monitor_thread, NULL);
    
    return 0;
}

