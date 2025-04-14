#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Polynomial for CRC-32 (Ethernet CRC-32: 0x04C11DB7)
#define CRC32_POLY 0x04C11DB7

// Function to calculate CRC-32 over the first 6 bytes
uint32_t calculate_crc32(uint8_t *data) {
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < 6; i++) {
        crc ^= (uint32_t)data[i] << 24;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) 
                crc = (crc << 1) ^ CRC32_POLY;
            else 
                crc <<= 1;
        }
    }
    return crc ^ 0xFFFFFFFF; // Final XOR
}

// Function to set CRC-32 in the last 4 bytes of the array
void set_crc32(uint8_t *frame) {
    uint32_t crc = calculate_crc32(frame);
    frame[6] = (crc >> 24) & 0xFF;
    frame[7] = (crc >> 16) & 0xFF;
    frame[8] = (crc >> 8) & 0xFF;
    frame[9] = crc & 0xFF;
}

// Function to verify CRC-32
void verify_crc32(uint8_t *frame) {
    uint32_t calculated_crc = calculate_crc32(frame);
    uint32_t received_crc = (frame[6] << 24) | (frame[7] << 16) | (frame[8] << 8) | frame[9];

    if (calculated_crc != received_crc) {
        printf("CRC-32 error! Exiting...\n");
        exit(EXIT_FAILURE);
    } else {
        printf("CRC-32 is valid.\n");
    }
}

// Example usage
int main() {
    uint8_t example_frame[10] = {0x05, 0x0A, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00}; // Last 4 bytes for CRC

    // Compute and set CRC
    set_crc32(example_frame);

    printf("Updated frame with CRC-32: ");
    for (int i = 0; i < 10; i++) {
        printf("%02X ", example_frame[i]);
    }
    printf("\n");

    // Verify CRC
    verify_crc32(example_frame);

    return 0;
}

