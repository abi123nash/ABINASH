#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>

#define MAX_LINE 256
#define MAX_DEVICES 10  // Max number of devices to track
#define PING_TIMEOUT 3   // Timeout for ping in seconds
#define MAX_WARNINGS 10  // Max number of warnings to store

typedef struct {
    char dev_name[32];
    char dev_ip[16];
} DeviceInfo;

DeviceInfo device_list[MAX_DEVICES];
int num_device_list = 0;
char warnings[MAX_WARNINGS][MAX_LINE];
char reply[MAX_WARNINGS][MAX_LINE];
int num_warnings = 0;  // To keep track of the number of warnings
int num_reply = 0;  // To keep track of the number of warnings
int i;
int first = 0;  // Flag to check if the file has been read before

// Function to ping an IP address
int ping_device(const char *ip_address) {
    char command[MAX_LINE];
   
    // Use -c 1 to send a single ping and -W 3 to set a timeout of 3 seconds
    snprintf(command, sizeof(command), "ping -c 1 -W %d %s > /dev/null 2>&1", PING_TIMEOUT, ip_address);

    return system(command);  // Returns 0 if successful, non-zero if failure
}

// Function to read device information from the file
void file_read() {
    FILE *file = fopen("client_information.txt", "r");

    // Check if file exists
    if (!file) {
        perror("Error: client_information.txt not found");
        exit(EXIT_FAILURE);  // Exit the application if file is not found
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), file)) {
        char dev_name[32], dev_ip[16];
        // Assuming the format in the file is: "Im <device_name> <ip_address>"
        if (sscanf(line, "Im %31s %15s", dev_name, dev_ip) == 2) {
            // Save the device information in the device list
            strcpy(device_list[num_device_list].dev_name, dev_name);
            strcpy(device_list[num_device_list].dev_ip, dev_ip);
            num_device_list++;
        }
    }

    fclose(file);  // Close the file after reading
}

// Function to send a message to a device over TCP and wait for a reply
int send_message_to_device(const char *ip_address) {
    int sockfd;
    struct sockaddr_in server_addr;
    char message[] = "Hello Server";
    char buffer[MAX_LINE];
    struct timeval timeout;
    fd_set readfds;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);  // Replace with actual port
    server_addr.sin_addr.s_addr = inet_addr(ip_address);

    // Set a timeout for receiving data
    timeout.tv_sec = 3;  // 3 seconds timeout
    timeout.tv_usec = 0;

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(sockfd);
        return -1;
    }

    // Send the message
    send(sockfd, message, strlen(message), 0);

    // Wait for a reply
    FD_ZERO(&readfds);
    FD_SET(sockfd, &readfds);

    int select_result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

    if (select_result > 0) {
        // Data is ready to be read
        int bytes_received = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            // Limit the size of the formatted string
            snprintf(reply[num_reply], MAX_LINE, "%d> %.243s", i + 1, buffer);  // Limit reply length
            ++num_reply;  // Increment the reply count
        } else {
            printf("No valid response from %s\n", ip_address);
        }
    } else if (select_result == 0) {
        printf("No reply from %s within timeout\n", ip_address);
    } else {
        perror("Error with select");
    }

    close(sockfd);
    return 0;
}

// Function to clear the console screen quickly
void clear_screen() {
    // ANSI escape codes to clear the screen and move cursor to the top
    printf("\033[H\033[J");
}

int main() {
    // Read device information from the file
    if (!first) {
        file_read();
        first = 1;
    }

    while (1) {
        
        num_warnings = 0;  // Clear previous warnings
        num_reply = 0;  // Clear previous replies

        // Loop through the device list and ping each device
        for (i = 0; i < num_device_list; i++) {

            // Ping the device
            if (ping_device(device_list[i].dev_ip) == 0) {
                // Attempt TCP connection and send message
                if (send_message_to_device(device_list[i].dev_ip) != 0) {
                    snprintf(warnings[num_warnings], MAX_LINE, "Warning: %d> %s IP: %s TCP connection failed.",i+1, device_list[i].dev_name, device_list[i].dev_ip);
                    num_warnings++;
                }
            } else {
                snprintf(warnings[num_warnings], MAX_LINE, "Warning: %d> %s IP: %s is not reachable.",i+1, device_list[i].dev_name, device_list[i].dev_ip);
                num_warnings++;
            }
        }
       clear_screen();
        // Print reply
        printf("Updated Device States:\n\n");
        for (int i = 0; i < num_reply; i++) {
            printf("%s\n", reply[i]);
        }
        
        // Print warnings
        for (int i = 0; i < num_warnings; i++) {
            printf("%s\n", warnings[i]);
        }

        sleep(2);  // Wait for 1 second before the next iteration
    }

    return 0;
}

