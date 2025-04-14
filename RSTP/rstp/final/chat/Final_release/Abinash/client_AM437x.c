#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>

#define MAX_LINE 256
#define SERVER_PORT 12345  // Port number
#define MAX_DEVICES 10     // Max number of devices to track
#define CMD "brctl showstp br0"
#define PORT1 "8001"  // eno1
#define PORT2 "8002"  // enx30de4b49af5e
#define BRIDGE_INTERFACE "br0"

int PINS[] = {122, 123};
int num_pins = 2;



// Board details
char BOARD_NAME[100];

// Structure to store port status information
typedef struct {
    char port_id[10];
    char state[20];
} PortStatus;


/** @brief is_valid_ip
 *
 *  This function checks if a given IP address string is a valid IPv4 address and not the localhost IP address (127.0.0.1).
 *
 *  @param ip_str : The IP address string to be checked.
 *
 *  @return int   : Returns 1 if the IP address is valid, 0 if invalid or if it's the localhost IP (127.0.0.1).
 */
int is_valid_ip(const char *ip_str)
{
    struct sockaddr_in sa;

    /*
     * Print the IP for debugging
     */
    printf("Checking IP: '%s'\n", ip_str);

    /*
     * Check if the IP is a valid IPv4 address
     */
    if (inet_pton(AF_INET, ip_str, &(sa.sin_addr)) != 1)
    {
        return 0; // Invalid IP address format
    }

    /*
     * Also check if it's not the localhost IP address (127.0.0.1)
     */
    if (strcasecmp(ip_str, "127.0.0.1") == 0)
    {
        return 0; // It's localhost, not a valid bridge IP
    }

    return 1; // Valid IP
}




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
    PortStatus status1, status2;
    char current_ip[50];
    char *ip;

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

        /*
         * Get the current IP address of br0
         */
        if (get_br0_ip(current_ip) == 0)
        {
            printf("Current IP of br0: %s\n", current_ip);

            /*
             * Remove the "addr:" prefix
             */
            ip = strchr(current_ip, ':');
            if (ip != NULL)
            {
                ip++; // Move past the colon
            }
            else
            {
                ip = current_ip; // No "addr:" prefix found, use the whole string
            }

            /*
             * Check if the current IP is valid
             */
            if (is_valid_ip(ip))
            {
                printf("IP: Valid\n");
            }
            else
            {
                printf("IP: Invalid\n");
            }
        }
        else
        {
            printf("Failed to get IP of br0\n");
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

/* LED start */

/** @brief export_gpio
 *
 *  This function exports a GPIO pin so that it can be used by the system.
 *
 *  @param pin : The GPIO pin number to be exported.
 *
 *  @return void : This function does not return any value.
 */
void export_gpio(int pin) 
{
    FILE *fp = fopen("/sys/class/gpio/export", "w");
    if (fp == NULL)
    {
        perror("Failed to export GPIO pin");
        exit(1);
    }
    fprintf(fp, "%d", pin);
    fclose(fp);
}

/** @brief set_gpio_direction
 *
 *  This function sets the direction (input or output) for a specified GPIO pin.
 *
 *  @param pin       : The GPIO pin number whose direction is to be set.
 *  @param direction : The direction to be set for the pin (either "in" or "out").
 *
 *  @return void : This function does not return any value.
 */
void set_gpio_direction(int pin, const char *direction) 
{
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
    {
        perror("Failed to set GPIO direction");
        exit(1);
    }
    fprintf(fp, "%s", direction);
    fclose(fp);
}

/** @brief write_gpio_value
 *
 *  This function writes a value (0 or 1) to a GPIO pin.
 *
 *  @param pin   : The GPIO pin number to which the value is written.
 *  @param value : The value to be written to the GPIO pin (either 0 or 1).
 *
 *  @return void : This function does not return any value.
 */
void write_gpio_value(int pin, int value) 
{
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    FILE *fp = fopen(path, "w");
    if (fp == NULL)
    {
        perror("Failed to write GPIO value");
        exit(1);
    }
    fprintf(fp, "%d", value);
    fclose(fp);
}

/** @brief unexport_gpio
 *
 *  This function unexports a GPIO pin, making it no longer accessible by the system.
 *
 *  @param pin : The GPIO pin number to be unexported.
 *
 *  @return void : This function does not return any value.
 */
void unexport_gpio(int pin) 
{
    FILE *fp = fopen("/sys/class/gpio/unexport", "w");
    if (fp == NULL)
    {
        perror("Failed to unexport GPIO pin");
        exit(1);
    }
    fprintf(fp, "%d", pin);
    fclose(fp);
}

/** @brief cleanup
 *
 *  This function handles GPIO cleanup by setting all configured pins to a low value (0) and unexporting them.
 *  It is typically called when the program exits to ensure proper cleanup of GPIO resources.
 *
 *  @param signum : The signal number (e.g., SIGINT) that triggered the cleanup.
 *
 *  @return void : This function does not return any value. It exits the program after cleanup.
 */
void cleanup(int signum) 
{
    for (int i = 0; i < num_pins; i++)
    {
        write_gpio_value(PINS[i], 0);  // Set pin value to 0 (off)
        unexport_gpio(PINS[i]);        // Unexport the GPIO pin
    }
    printf("GPIO cleanup complete.\n");
    exit(0);
}


/** @brief check_stp_status
 *
 *  This function checks if Spanning Tree Protocol (STP) is enabled for the `br0` bridge using the `brctl show` command.
 *
 *  @return int : Returns 1 if STP is enabled, 0 if STP is not enabled, and -1 if an error occurs or `br0` is not found.
 */
int check_stp_status() 
{
    FILE *fp;
    char line[256];

    /*
     * Open the `brctl show` command to check the STP status for the bridge
     */
    fp = popen("brctl show", "r");
    if (fp == NULL) 
    {
        perror("Error opening brctl show command");
        return -1;
    }

    /*
     * Read each line to check for the bridge interface and STP status
     */
    while (fgets(line, sizeof(line), fp)) 
    {
        /*
         * Check if the line contains the bridge name (br0) and STP status
         */
        if (strstr(line, BRIDGE_INTERFACE) != NULL) 
        {
            /*
             * Check if STP is enabled by looking for "yes" in the third column
             */
            if (strstr(line, "yes") != NULL) 
            {
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

/** @brief check_ports_status
 *
 *  This function checks the status of the ports for the `br0` bridge using the `brctl showstp br0` command.
 *  It checks whether any ports are in a "disabled" state.
 *
 *  @return int : Returns 1 if any port is disabled, 0 if all ports are active, and -1 if an error occurs.
 */
int check_ports_status() 
{
    FILE *fp;
    char line[256];
    int disabled_ports = 0;

    /*
     * Open the `brctl showstp br0` command to check the port status for the bridge
     */
    fp = popen("brctl showstp br0", "r");
    if (fp == NULL) 
    {
        perror("Error opening brctl showstp command");
        return -1;
    }

    /*
     * Check each port for its state (forwarding, blocking, disabled)
     */
    while (fgets(line, sizeof(line), fp)) 
    {
        if (strstr(line, "state")) 
        {
            /*
             * Check the state of the port
             */
            if (strstr(line, "disabled") != NULL) 
            {
                printf("Port state: Disabled (Not OK)\n");
                disabled_ports = 1;
            }
        }
    }

    fclose(fp);
    return disabled_ports;
}

/** @brief check_rstp_status
 *
 *  This function checks if Rapid Spanning Tree Protocol (RSTP) is enabled on the `br0` bridge using the `mstpctl showbridge br0` command.
 *
 *  @return int : Returns 1 if RSTP is enabled, 0 if RSTP is not enabled, and -1 if an error occurs.
 */
int check_rstp_status()
{
    FILE *fp;
    char line[256];

    /*
     * Open the `mstpctl showbridge br0` command to check the RSTP status for the bridge
     */
    fp = popen("mstpctl showbridge br0", "r");
    if (fp == NULL)
    {
        perror("Error opening mstpctl showbridge command");
        return -1;
    }

    /*
     * Search for "force protocol version" and check if it's set to "rstp"
     */
    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, "force protocol version") != NULL)
        {
            if (strstr(line, "rstp") != NULL)
            {
                fclose(fp);
                return 1;  // RSTP is enabled
            }
        }
    }

    fclose(fp);
    return 0;  // RSTP is not enabled
}

/** @brief check_rstp_status_main
 *
 *  This function monitors the status of the `br0` bridge, including checking its IP address, STP status, port status, and RSTP status.
 *  It repeatedly checks these statuses every 3 seconds and prints the results.
 *
 *  @return int : Returns 1 if the bridge configuration is OK (valid IP, STP enabled, no disabled ports, and RSTP enabled),
 *               and 2 if the bridge configuration is not OK.
 */
int check_rstp_status_main()
{
    char current_ip[50];
    char *ip;
    int stp_status;
    int port_status;
    int rstp_status;

    while (1)
    {
        /*
         * Get the current IP address of br0
         */
        if (get_br0_ip(current_ip) == 0)
        {
            printf("Current IP of br0: %s\n", current_ip);

            /*
             * Remove the "addr:" prefix
             */
            ip = strchr(current_ip, ':');
            if (ip != NULL)
            {
                ip++; // Move past the colon
            }
            else
            {
                ip = current_ip; // No "addr:" prefix found, use the whole string
            }

            /*
             * Check if the current IP is valid
             */
            if (is_valid_ip(ip))
            {
                printf("IP: Valid\n");
            }
            else
            {
                printf("IP: Invalid\n");
            }
        }
        else
        {
            printf("Failed to get IP of br0\n");
        }

        /*
         * Check the STP status of the br0 bridge
         */
        stp_status = check_stp_status();
        if (stp_status == 1)
        {
            printf("STP: Enabled\n");
        }
        else if (stp_status == 0)
        {
            printf("STP: Not Enabled\n");
        }
        else
        {
            printf("Failed to check STP status\n");
        }

        /*
         * Check the port status of br0
         */
        port_status = check_ports_status();
        if (port_status == 0)
        {
            printf("All ports are in a valid state (Forwarding or Blocking)\n");
        }
        else
        {
            printf("One or more ports are disabled, Bridge config not OK\n");
        }

        /*
         * Check if RSTP is enabled
         */
        rstp_status = check_rstp_status();
        if (rstp_status == 1)
        {
            printf("RSTP: Enabled\n");
        }
        else if (rstp_status == 0)
        {
            printf("RSTP: Not Enabled\n");
        }
        else
        {
            printf("Failed to check RSTP status\n");
        }

        /*
         * Check if both IP is valid, STP is enabled, no port is disabled, and RSTP is enabled
         */
        if (is_valid_ip(ip) && stp_status == 1 && port_status == 0 && rstp_status == 1)
        {
            printf("Bridge config OK\n\n");
            return 1;
        }
        else
        {
            printf("Bridge config not OK\n\n");
            return 2;
        }

        /*
         * Sleep for a while before checking again
         */
    }
}

/** @brief control_leds
 *
 *  This function controls the LEDs based on the RSTP status of the bridge.
 *  It uses GPIO pins to turn the LEDs on or off.
 *  The LEDs are controlled by setting the GPIO pins to either high (1) or low (0).
 *
 *  @param arg : Argument passed to the function (not used here).
 *
 *  @return NULL : The function runs indefinitely until interrupted.
 */
void *control_leds(void *arg) {

    /*
     * Set up SIGINT (Ctrl+C) signal handler for cleanup
     */
    signal(SIGINT, cleanup);

    /*
     * Export the GPIO pins and set them as outputs
     */
    for (int i = 0; i < num_pins; i++) {
        export_gpio(PINS[i]);               // Export GPIO pin
        set_gpio_direction(PINS[i], "out"); // Set GPIO direction to output
        write_gpio_value(PINS[i], 1);       // Set GPIO value to high (on)
    }

    printf("Starting LED blink test. Press Ctrl+C to stop.\n");

    /*
     * Blink the LEDs based on the RSTP status
     */
    while (1) {

        int ret = check_rstp_status_main(); // Check RSTP status

        /*
         * If RSTP is enabled, turn on green LED, turn off red LED
         */
        if (ret == 1) {
            write_gpio_value(PINS[1], 1);  // Green LED (OK)
            write_gpio_value(PINS[0], 0);  // Red LED (Off)
        }
        /*
         * If RSTP is not enabled, turn on red LED, turn off green LED
         */
        else if (ret == 2) {
            write_gpio_value(PINS[0], 1);  // Red LED (Not OK)
            write_gpio_value(PINS[1], 0);  // Green LED (Off)
        }
        sleep(1); // Check every 3 seconds
    }

    return NULL;
}


int main() {

    pthread_t led_control_thread;
   
    /*
     * Start LED control in a separate thread
     */
    if (pthread_create(&led_control_thread, NULL, control_leds, NULL) != 0) {
        perror("Failed to create LED control thread");
        return EXIT_FAILURE;
    }


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

        /*
     * Join threads (this won't actually execute since udp_client runs indefinitely)
     */
    pthread_join(led_control_thread, NULL);
    // Close the server socket
    close(sockfd);
    return 0;
}

