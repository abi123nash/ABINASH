#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Polynomial for CRC-8 (CRC-8-CCITT: x^8 + x^2 + x^1 + 1 -> 0x07)
#define CRC8_POLY 0x07

// Function to calculate CRC-8 over the first 6 bytes
uint8_t calculate_crc8(uint8_t *data) {
    uint8_t crc = 0x00;
    for (int i = 0; i < 6; i++) {
        crc ^= data[i];  // XOR with byte
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) 
                crc = (crc << 1) ^ CRC8_POLY;
            else 
                crc <<= 1;
        }
    }
    return crc;
}

// Function to set CRC-8 in the last byte of the array
void set_crc8(uint8_t *frame) {
    frame[6] = calculate_crc8(frame);
}

// Function to verify CRC-8
void verify_crc8(uint8_t *frame) {
    uint8_t calculated_crc = calculate_crc8(frame);
    if (calculated_crc != frame[6]) {
        printf("CRC-8 error! Exiting...\n");
        exit(EXIT_FAILURE);
    } else {
        printf("CRC-8 is valid.\n");
    }
}

// Example usage
int main() {
    uint8_t example_frame[7] = {0x05, 0x0A, 0x00, 0x03, 0x00, 0x01, 0x00};  // Last byte is CRC

    // Compute and set CRC
    set_crc8(example_frame);

    printf("Updated frame with CRC-8: ");
    for (int i = 0; i < 7; i++) {
        printf("%02X ", example_frame[i]);
    }
    printf("\n");

    // Verify CRC
    verify_crc8(example_frame);

    return 0;
}

