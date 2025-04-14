#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define MAX_DATA_LEN 8

void print_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void setup_can_interface(const char *interface, int bitrate) {
    char command[128];


 // Set up the CAN interface with the specified bitrate
    snprintf(command, sizeof(command), "ip link set %s down", interface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }


    // Set up the CAN interface with the specified bitrate
    snprintf(command, sizeof(command), "ip link set %s up type can bitrate %d", interface, bitrate);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    printf("Successfully set up CAN interface %s with bitrate %d\n", interface, bitrate);
    
    // Set up the CAN interface with the specified bitrate
    snprintf(command, sizeof(command), "ip link set %s up", interface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }
    
    printf("Successfully %s ip link set up\n", interface);
}

void send_can_message(int socket_fd, struct can_frame *frame) {
    if (write(socket_fd, frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        print_error("Error sending CAN frame");
    }
    printf("CAN frame sent successfully: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");
}

int main() {
    const char *can_interface = "can0";
    int bitrate = 125000;
    int socket_fd;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;

    // Set up the CAN interface
    setup_can_interface(can_interface, bitrate);

    // Create a socket for CAN communication
    if ((socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        print_error("Socket creation failed");
    }

    // Specify the CAN interface to use
    strcpy(ifr.ifr_name, can_interface);
    if (ioctl(socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        print_error("Error getting CAN interface index");
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind the socket to the CAN interface
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        print_error("Error binding socket to CAN interface");
    }

    printf("Connected to CAN interface %s\n", can_interface);

    while (1) {
        // Ask the user for input
        unsigned int can_id;
        char data[MAX_DATA_LEN] = {0};
        int dlc;

        printf("\nEnter CAN ID (in hex, e.g., 123): ");
        if (scanf("%x", &can_id) != 1) {
            fprintf(stderr, "Invalid CAN ID\n");
            continue;
        }

        printf("Enter Data Length Code (0-8): ");
        if (scanf("%d", &dlc) != 1 || dlc < 0 || dlc > MAX_DATA_LEN) {
            fprintf(stderr, "Invalid DLC\n");
            continue;
        }

        printf("Enter Data (space-separated hex, e.g., DE AD BE EF): ");
        for (int i = 0; i < dlc; i++) {
            unsigned int byte;
            if (scanf("%x", &byte) != 1 || byte > 0xFF) {
                fprintf(stderr, "Invalid data byte\n");
                dlc = 0; // Reset DLC in case of error
                break;
            }
            data[i] = (unsigned char)byte;
        }

        if (dlc == 0) {
            fprintf(stderr, "Data entry failed\n");
            continue;
        }

        // Prepare the CAN frame
        frame.can_id = can_id;
        frame.can_dlc = dlc;
        memcpy(frame.data, data, dlc);

        // Send the CAN frame
        send_can_message(socket_fd, &frame);
    }

    // Close the socket
    close(socket_fd);
    return 0;
}

