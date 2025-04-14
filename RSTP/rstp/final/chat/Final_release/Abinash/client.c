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
#define SERVER_PORT 12345  // Port number
#define MAX_DEVICES 10     // Max number of devices to track
#define CMD "brctl showstp br0"
#define PORT1 "8001"  // eno1
#define PORT2 "8002"  // enx30de4b49af5e


// Board details
char BOARD_NAME[100];

// Structure to store port status information
typedef struct {
    char port_id[10];
    char state[20];
} PortStatus;



int get_device_name()
{
    FILE *fd;

    /* 
     * Read the device name from the dev.txt file 
     */
    fd = fopen((const char *)"dev.txt", "r");

    if (NULL == fd)
    {
        printf("Failed to open %s \r\n", "dev.txt");
        return 0;
    }

    /* 
     * Read the device name from the file 
     */
    fgets((char *)BOARD_NAME, 100 , fd);

    /* 
     * Remove new line char 
     */
    BOARD_NAME[strlen((const char *)BOARD_NAME) - 1] = '\0';
    printf("Device Name %s \r\n", BOARD_NAME);
    fclose(fd);

}




/** @brief get_port_status
 *
 *  Retrieves the status of two network ports by executing a command
 *  and parsing the output.
 *
 *  @param status1 : Pointer to PortStatus struct for the first port
 *  @param status2 : Pointer to PortStatus struct for the second port
 *
 *  @return int : Returns 0 on success, -1 on failure
 */
int get_port_status(PortStatus *status1, PortStatus *status2)
{
    FILE *fp;
    char line[MAX_LINE];
    int found1 = 0, found2 = 0;

    /*
     * Open command output
     */
    fp = popen(CMD, "r");
    if (!fp)
    {
        perror("popen failed");
        return -1;
    }

    /*
     * Read and parse output
     */
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, "port id"))
        {
            char temp_id[10], temp_state[20];

            /*
             * Extract port ID and state
             */
            if (sscanf(line, " port id %s state %s", temp_id, temp_state) == 2)
            {
                if (strcmp(temp_id, PORT1) == 0)
                {
                    strcpy(status1->port_id, temp_id);
                    strcpy(status1->state, temp_state);
                    found1 = 1;
                }
                else if (strcmp(temp_id, PORT2) == 0)
                {
                    strcpy(status2->port_id, temp_id);
                    strcpy(status2->state, temp_state);
                    found2 = 1;
                }
            }
        }
    }

    /*
     * Close the command output
     */
    pclose(fp);

    /*
     * Return success if both ports were found
     */
    return (found1 && found2) ? 0 : -1;
}

// Function to get the device IP (e.g., from `ifconfig`)
int get_br0_ip(char *ip) {
    FILE *fp;
    char line[256];
    char *ip_start;

    // Open the `ifconfig` command to get the IP of the `br0` interface
    fp = popen("ifconfig br0", "r");
    if (fp == NULL) {
        perror("Error opening ifconfig command");
        return -1;
    }

    // Read the output of the ifconfig command line by line
    while (fgets(line, sizeof(line), fp)) {
        if ((ip_start = strstr(line, "inet ")) != NULL) {
            sscanf(ip_start, "inet %s", ip);
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return -1;
}

// Function to handle a client connection
void handle_client(int client_sock) {
    char buffer[MAX_LINE];
    char ip[16];
    PortStatus status1, status2;

    // Receive message from the client
    int recv_len = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (recv_len < 0) {
        perror("Receive failed");
        close(client_sock);
        return;
    }
    buffer[recv_len] = '\0';  // Null-terminate the received message

    // Check if the message matches the expected pattern
    if (strcmp(buffer, "Hello Server") == 0) {
        // Get the board name and IP address
        if (get_br0_ip(ip) != 0) {
            strcpy(ip, "Unknown IP");
        }
         
        get_device_name();

        // Get port status
        if (get_port_status(&status1, &status2) == 0) {
            // Prepare the message to send back
	     snprintf(buffer, sizeof(buffer), "%-13s    IP:%-20s    Port %s - State: %-13s      Port %s - State: %s\n",
                 BOARD_NAME, ip, status1.port_id, status1.state, status2.port_id, status2.state);
        } else {
            //snprintf(buffer, sizeof(buffer), "%-13s IP: %-20s Error fetching port status.\n",BOARD_NAME, ip);
        }

        // Send the response back to the client
        send(client_sock, buffer, strlen(buffer), 0);
    } else {
        printf("Received invalid message: %s\n", buffer);
    }

    // Close the connection
    close(client_sock);
}


int main() {

    int sockfd, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Bind socket to address
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sockfd, 5) < 0) {
        perror("Listen failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", SERVER_PORT);

    while (1) {
        // Accept incoming client connection
        client_sock = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));

        // Handle the client
        handle_client(client_sock);
    }

    // Close the server socket
    close(sockfd);
    return 0;
}

