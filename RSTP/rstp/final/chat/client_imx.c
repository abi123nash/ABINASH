#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define CMD "brctl showstp br0"
#define BOARD_NAME "Im IMX93"
#define MAX_LINE 256
#define PORT1 "8001"  // eno1
#define PORT2 "8002"  // enx30de4b49af5e
#define UDP_PORT_RX 12345
#define UDP_PORT_TX 1234
#define BUFFER_SIZE 1024

char sender_ip[INET_ADDRSTRLEN];
uint8_t server_connect = 0;
int flag=0;

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
int send_port_status(const char *sender_ip, const PortStatus *status1, const PortStatus *status2) {
    int sockfd;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int status=0;
    struct timeval timeout;
    fd_set read_fds;
    char ack_buffer[MAX_LINE];  // Buffer for ACK response
    int ret;

    /* Check if the server is connected */
    if (server_connect == 0) {
        printf("Server not connected, retrying...\n");
        return -1;
    }

    /* Create UDP socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    /* Configure server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(UDP_PORT_TX);
    if (inet_pton(AF_INET, sender_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid sender IP address");
        close(sockfd);
        return -1;
    }

    /* Prepare stmonitor_portsatus message */
    snprintf(buffer, sizeof(buffer), "%s Port %s - State: %s\nPort %s - State: %s\n", BOARD_NAME,
             status1->port_id, status1->state, status2->port_id, status2->state);

   int retries = 1;
   
   while(1)
   {

      if(retries >= 5)
      {
	 status=-1;     
	 break;     
      }      

    /* Send the message */
    if (sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Send failed");
	retries++;
	continue;  // Keep the server running
    } else {
        printf("Port status sent to %s:%d\n", sender_ip, UDP_PORT_TX);
    }
      
      /* Wait for ACK with timeout (5 seconds) */
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;

    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret == -1) {
        perror("Select failed");
        close(sockfd);
        return -1;
    } else if (ret == 0) {
        printf("Timeout reached, no ACK received. Retrying...\n");
        // Timeout, resend the message
	retries++;
	continue;  // Keep the server running
    } else {
        if (FD_ISSET(sockfd, &read_fds)) {
            // Read ACK message
            int ack_len = recvfrom(sockfd, ack_buffer, sizeof(ack_buffer) - 1, 0, NULL, NULL);
            if (ack_len < 0) {
                perror("Recvfrom failed for ACK");
                close(sockfd);
                return -1;
            }
            ack_buffer[ack_len] = '\0';
            
            if (strcmp(ack_buffer, "ACK") == 0) {
                printf("Received ACK from server\n");
		break;
            } else {
                printf("Unexpected message received: %s\n", ack_buffer);
	        retries++;
	        continue;  // Keep the server running
            }
        }
    }


  }

    close(sockfd);
    return status;
}

/* Monitoring function (runs as a separate thread) */
void *monitor_ports(void *arg) {
    PortStatus prev_status1 = {"", ""}, prev_status2 = {"", ""};
    PortStatus curr_status1, curr_status2;

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

        if (flag == -1) {  

            flag = send_port_status(sender_ip, &curr_status1, &curr_status2);
        }
    }

    return NULL;
}

/* UDP Client Function */
void udp_client() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    struct timeval timeout;
    fd_set readfds;

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

    while (1) {
         FD_ZERO(&readfds);
         FD_SET(sockfd, &readfds);

         timeout.tv_sec = 2; // Timeout of 3 seconds
         timeout.tv_usec = 0;

         int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

         if (activity == 0) {
            // Timeout occurred, no message received within 2 seconds
             printf("No message received in the last 2 seconds.\n");
             flag = -1; // Change the flag here
             server_connect = 0; // Update the flag if a message is received
        } else if (activity < 0) {
            perror("Select error");
        } else {
            // Data is available to read
            if (FD_ISSET(sockfd, &readfds)) {
                socklen_t addr_len = sizeof(server_addr);
                int recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&server_addr, &addr_len);
                if (recv_len > 0) {
                    buffer[recv_len] = '\0';
                    printf("I'm the B device\n");
                    printf("Received: %s\n", buffer);

                    // Get the sender's IP address
                    inet_ntop(AF_INET, &server_addr.sin_addr, sender_ip, sizeof(sender_ip));
                    printf("Message received from %s:%d\n", sender_ip, UDP_PORT_RX);
                    server_connect = 1; // Update the flag if a message is received
                }
            }
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

