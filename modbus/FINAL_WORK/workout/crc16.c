#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Polynomial for CRC-16 (MODBUS)
#define CRC_POLY 0xA001

// Function to calculate CRC-16 over the first 6 bytes
uint16_t calculate_crc16(uint8_t *data) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < 6; i++) {
        crc ^= data[i];  // XOR with byte
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ CRC_POLY;
            else
                crc >>= 1;
        }
    }
    return crc;
}

// Function to set CRC in the last 2 bytes of the array
void set_crc(uint8_t *frame) {
    uint16_t crc = calculate_crc16(frame);
    frame[6] = (crc >> 8) & 0xFF;  // High byte
    frame[7] = crc & 0xFF;         // Low byte
}

// Function to verify CRC by comparing calculated and received CRC
void verify_crc16(uint8_t *frame) {
    uint16_t calculated_crc = calculate_crc16(frame);
    uint16_t received_crc = (frame[6] << 8) | frame[7];  // Extract CRC from frame

    if (calculated_crc != received_crc) {
        printf("CRC error! Exiting...\n");
        exit(EXIT_FAILURE);
    } else {
        printf("CRC is valid.\n");
    }
}

// Example usage
int main() {
    uint8_t example_frame[8] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};  // Initial frame with empty CRC

    // Compute and set CRC
    set_crc(example_frame);

    printf("Updated frame with CRC: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X ", example_frame[i]);
    }
    printf("\n");
    
    // Verify CRC
    verify_crc16(example_frame);

    return 0;
}

