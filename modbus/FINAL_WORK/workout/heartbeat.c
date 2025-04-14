#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <linux/can.h>   // For CAN frame structure

int main() {
    uint32_t uptime = 0;
    struct can_frame heartbeat_msg;

    heartbeat_msg.can_id = 0x017E0333;  // CAN ID for heartbeat
    heartbeat_msg.can_dlc = 8;          // Data length

    while (1) {
        // Set the uptime value in the first 4 bytes
        heartbeat_msg.data[0] = (uint8_t)(uptime >> 24);
        heartbeat_msg.data[1] = (uint8_t)(uptime >> 16);
        heartbeat_msg.data[2] = (uint8_t)(uptime >> 8);
        heartbeat_msg.data[3] = (uint8_t)uptime;

        // Set remaining bytes to 0
        memset(&heartbeat_msg.data[4], 0, 4);

        // Print the CAN frame data (CAN ID and data bytes)
        printf("CAN ID: 0x%X\n", heartbeat_msg.can_id);
        printf("Data: ");
        for (int i = 0; i < 8; i++) {
            printf("0x%02X ", heartbeat_msg.data[i]);
        }
        printf("\n");

        uptime++;  // Increment uptime
        usleep(500000);  // Sleep for 500ms
    }

    return 0;
}

