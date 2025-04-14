#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BRIDGE_INTERFACE "br0"

// Function to check if an IP address is valid (not localhost and properly formatted)
int is_valid_ip(const char *ip_str) {
    struct sockaddr_in sa;
    // Check if the IP is a valid IPv4 address
    if (inet_pton(AF_INET, ip_str, &(sa.sin_addr)) != 1) {
        return 0; // Invalid IP address format
    }
    
    // Also check if it's not the localhost IP address (127.0.0.1)
    if (strcmp(ip_str, "127.0.0.1") == 0) {
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

    fp = popen("sudo brctl show", "r");
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

    fp = popen("sudo brctl showstp br0", "r");
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

int main() {
    char current_ip[50];
    int stp_status;
    int port_status;
    
    while (1) {
        // Get the current IP address of br0
        if (get_br0_ip(current_ip) == 0) {
            printf("Current IP of br0: %s\n", current_ip);

            // Check if the current IP is valid
            if (is_valid_ip(current_ip)) {
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

        // Check if both IP is valid, STP is enabled, and no port is disabled
        if (is_valid_ip(current_ip) && stp_status == 1 && port_status == 0) {
            printf("Bridge config OK\n");
        } else {
            printf("Bridge config not OK\n");
        }

        // Sleep for a while before checking again
        sleep(5); // Check every 5 seconds
    }

    return 0;
}

