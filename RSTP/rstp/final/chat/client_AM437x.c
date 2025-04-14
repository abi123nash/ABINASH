#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define CMD "brctl showstp br0"
#define BOARD_NAME "Im AM437x"
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

/**************************************************************** LED start *************************************************************/
#include <signal.h>
int PINS[] = {122, 123, 124};
int num_pins = 3;
#define BRIDGE_INTERFACE "br0"

// Function to export a GPIO pin
void export_gpio(int pin) {
    FILE *fp = fopen("/sys/class/gpio/export", "w");
    if (fp == NULL) {
        perror("Failed to export GPIO pin");
        exit(1);
    }
    fprintf(fp, "%d", pin);
    fclose(fp);
}

// Function to set a GPIO pin direction
void set_gpio_direction(int pin, const char *direction) {
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("Failed to set GPIO direction");
        exit(1);
    }
    fprintf(fp, "%s", direction);
    fclose(fp);
}

// Function to write value to a GPIO pin
void write_gpio_value(int pin, int value) {
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("Failed to write GPIO value");
        exit(1);
    }
    fprintf(fp, "%d", value);
    fclose(fp);
}

// Function to unexport a GPIO pin
void unexport_gpio(int pin) {
    FILE *fp = fopen("/sys/class/gpio/unexport", "w");
    if (fp == NULL) {
        perror("Failed to unexport GPIO pin");
        exit(1);
    }
    fprintf(fp, "%d", pin);
    fclose(fp);
}

// Cleanup function to handle GPIO cleanup on exit
void cleanup(int signum) {
    for (int i = 0; i < num_pins; i++) {
        write_gpio_value(PINS[i], 0);  // Set pin value to 0 (off)
        unexport_gpio(PINS[i]);        // Unexport the GPIO pin
    }
    printf("GPIO cleanup complete.\n");
    exit(0);
}

int is_valid_ip(const char *ip_str) {
    struct sockaddr_in sa;

    // Print the IP for debugging
    printf("Checking IP: '%s'\n", ip_str);

    // Check if the IP is a valid IPv4 address
    if (inet_pton(AF_INET, ip_str, &(sa.sin_addr)) != 1) {
        return 0; // Invalid IP address format
    }

    // Also check if it's not the localhost IP address (127.0.0.1)
    if (strcasecmp(ip_str, "127.0.0.1") == 0) {
        return 0; // It's localhost, not a valid bridge IP
    }

    return 1; // Valid IP
}


// Function to get the IP address of br0 interface using `ifconfig`
int get_br0_ip(char *ip) {
    FILE *fp;
    char line[256];
    char *ip_start;

    fp = popen("ifconfig br0", "r");
    if (fp == NULL) {
        perror("Error opening ifconfig command");
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        // Look for the line that contains the inet (IPv4) address
        if ((ip_start = strstr(line, "inet ")) != NULL) {
            // Extract the IP address from the line
            sscanf(ip_start, "inet %s", ip);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

// Function to check if STP is enabled for the br0 bridge using `brctl show`
int check_stp_status() {
    FILE *fp;
    char line[256];

    fp = popen("brctl show", "r");
    if (fp == NULL) {
        perror("Error opening brctl show command");
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        // Check if the line contains the bridge name (br0) and STP status
        if (strstr(line, BRIDGE_INTERFACE) != NULL) {
            // Check if STP is enabled by looking at the "yes" in the third column
            if (strstr(line, "yes") != NULL) {
                fclose(fp);
                return 1;  // STP is enabled
            }
            fclose(fp);
            return 0;  // STP is not enabled
        }
    }

    fclose(fp);
    return -1;  // Error or no br0 bridge found
}

// Function to check the port status of br0 using `brctl showstp br0`
int check_ports_status() {
    FILE *fp;
    char line[256];
    int disabled_ports = 0;

    fp = popen("brctl showstp br0", "r");
    if (fp == NULL) {
        perror("Error opening brctl showstp command");
        return -1;
    }

    // Check each port for its state (forwarding, blocking, disabled)
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "state")) {
            // Check the state of the port
            if (strstr(line, "disabled") != NULL) {
                printf("Port state: Disabled (Not OK)\n");
                disabled_ports = 1;
            }
        }
    }

    fclose(fp);
    return disabled_ports;
}

// Function to check if RSTP is enabled using `mstpctl showbridge br0`
int check_rstp_status() {
    FILE *fp;
    char line[256];

    fp = popen("mstpctl showbridge br0", "r");
    if (fp == NULL) {
        perror("Error opening mstpctl showbridge command");
        return -1;
    }

    // Search for "force protocol version" and check if it's set to "rstp"
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "force protocol version") != NULL) {
            if (strstr(line, "rstp") != NULL) {
                fclose(fp);
                return 1;  // RSTP is enabled
            }
        }
    }

    fclose(fp);
    return 0;  // RSTP is not enabled
}


int check_rstp_status_main() 
{
    char current_ip[50];
    char *ip;
    int stp_status;
    int port_status;
    int rstp_status;
    
    while (1) {
        // Get the current IP address of br0
        if (get_br0_ip(current_ip) == 0) {
            printf("Current IP of br0: %s\n", current_ip);

           // Remove the "addr:" prefix
               ip = strchr(current_ip, ':');
           if (ip != NULL) {
                  ip++; // Move past the colon
             } else {
               ip = current_ip; // No "addr:" prefix found, use the whole string
            }

            // Check if the current IP is valid
            if (is_valid_ip(ip)) {
                printf("IP: Valid\n");
            } else {
                printf("IP: Invalid\n");
            }
        } else {
            printf("Failed to get IP of br0\n");
        }

        // Check the STP status of the br0 bridge
        stp_status = check_stp_status();
        if (stp_status == 1) {
            printf("STP: Enabled\n");
        } else if (stp_status == 0) {
            printf("STP: Not Enabled\n");
        } else {
            printf("Failed to check STP status\n");
        }

        // Check the port status of br0
        port_status = check_ports_status();
        if (port_status == 0) {
            printf("All ports are in a valid state (Forwarding or Blocking)\n");
        } else {
            printf("One or more ports are disabled, Bridge config not OK\n");
        }

        // Check if RSTP is enabled
        rstp_status = check_rstp_status();
        if (rstp_status == 1) {
            printf("RSTP: Enabled\n");
        } else if (rstp_status == 0) {
            printf("RSTP: Not Enabled\n");
        } else {
            printf("Failed to check RSTP status\n");
        }

        // Check if both IP is valid, STP is enabled, no port is disabled, and RSTP is enabled
        if (is_valid_ip(ip) && stp_status == 1 && port_status == 0 && rstp_status == 1) {
            printf("Bridge config OK\n\n");
            return 1;
        } else {
            printf("Bridge config not OK\n\n");
            return 2;
        }

        // Sleep for a while before checking again
        sleep(3); // Check every 5 seconds
    }

}


void *control_leds(void *arg) {
    
	    // Set up SIGINT (Ctrl+C) signal handler for cleanup
    signal(SIGINT, cleanup);

    // Export the GPIO pins and set them as outputs
    for (int i = 0; i < num_pins; i++) {
        export_gpio(PINS[i]);
        set_gpio_direction(PINS[i], "out");
	write_gpio_value(PINS[i], 1);

    }

    printf("Starting LED blink test. Press Ctrl+C to stop.\n");

    // Blink the LEDs
	while (1) {
   
          int ret =check_rstp_status_main();

           if(ret == 1)
	   {
	    write_gpio_value(PINS[1], 1);  // off red led all ok
	    write_gpio_value(PINS[0], 0);
	   }
	    else if(ret == 2)
            {		    
            write_gpio_value(PINS[0], 1); // off green led not ok
            write_gpio_value(PINS[1], 0);
	    }
  
	
	}
    return NULL;
}



/**************************************************************** LED END *************************************************************/

int main() {
    pthread_t monitor_thread,led_control_thread;

    /* Start monitoring ports in a separate thread */
    if (pthread_create(&monitor_thread, NULL, monitor_ports, NULL) != 0) {
        perror("Failed to create monitoring thread");
        return EXIT_FAILURE;
    }

   // Start LED control in a separate thread
    if (pthread_create(&led_control_thread, NULL, control_leds, NULL) != 0) {
        perror("Failed to create LED control thread");
        return EXIT_FAILURE;
    }

    /* Run UDP client in the main thread */
    udp_client();

    /* Join thread (this won't actually execute since udp_client runs indefinitely) */
    pthread_join(monitor_thread, NULL);
    pthread_join(led_control_thread, NULL);
    return 0;
}

