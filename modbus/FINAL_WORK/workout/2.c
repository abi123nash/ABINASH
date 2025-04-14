#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>

#define CAN_REQUEST_ID  0x01200333
#define CAN_RESPONSE_ID 0x01210333
#define CAN_ACK_ID      0x01290333
#define FRAME_DATA_SIZE 5
#define TOTAL_FRAMES    12

typedef struct __attribute__((packed)) {
    uint16_t data[23];
    uint32_t data24;
    uint32_t data25;
    uint16_t data26;
} BR_DATA;

// Function to send a CAN frame
void send_can_message(int socket_fd, uint32_t can_id, uint8_t *data, uint8_t dlc) {
    struct can_frame frame;
    frame.can_id  = can_id;
    frame.can_dlc = dlc;
    memcpy(frame.data, data, dlc);
    
    if (write(socket_fd, &frame, sizeof(frame)) != sizeof(frame)) {
        perror("Error sending CAN frame");
    }
}

// Function to receive a CAN frame
int receive_can_message(int socket_fd, struct can_frame *frame) {
    return read(socket_fd, frame, sizeof(struct can_frame)) == sizeof(struct can_frame);
}

// Function to validate CRC (simple XOR check for example, replace with actual CRC logic)
int validate_crc(uint8_t *data, uint8_t crc) {
    uint8_t checksum = 0;
    for (int i = 0; i < FRAME_DATA_SIZE; i++) {
        checksum ^= data[i];
    }
    return (checksum == crc);
}

// Function to handle CAN data reception and reassembly
void can_send_receive_reassemble_fragmented_data(int socket_fd) {
    struct can_frame frame;
    BR_DATA br_data = {0};
    int frame_index = 0;
    uint32_t expected_id = CAN_RESPONSE_ID;
    
    // Send read request
    send_can_message(socket_fd, CAN_REQUEST_ID, NULL, 0);
    
    while (frame_index < TOTAL_FRAMES) {
        // Receive response frame
        if (!receive_can_message(socket_fd, &frame) || frame.can_id != expected_id) {
            perror("Error receiving CAN frame");
            return;
        }
        
        // Validate CRC
        if (!validate_crc(frame.data, frame.data[6])) {
            fprintf(stderr, "CRC validation failed for frame %d\n", frame_index + 1);
            return;
        }
        
        // Store data in BR_DATA structure
        memcpy(((uint8_t *)&br_data) + frame_index * FRAME_DATA_SIZE, &frame.data[1], FRAME_DATA_SIZE);
        
        // Send acknowledgment
        send_can_message(socket_fd, CAN_ACK_ID + (frame_index * FRAME_DATA_SIZE), frame.data, 8);
        
        // Increment expected ID and frame index
        expected_id += FRAME_DATA_SIZE;
        frame_index++;
    }
    
    printf("Data received and stored successfully.\n");
}

