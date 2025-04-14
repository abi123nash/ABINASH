#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define INTERFACE "br0"

// Function to get the incoming and outgoing bytes of the interface
int get_network_stats(unsigned long *rx_bytes, unsigned long *tx_bytes) {
    FILE *fp;
    char line[256];
    unsigned long rx, tx;

    // Open the file that contains the network statistics
    fp = fopen("/sys/class/net/" INTERFACE "/statistics/rx_bytes", "r");
    if (fp == NULL) {
        perror("Error opening rx_bytes");
        return -1;
    }
    fscanf(fp, "%lu", &rx);
    fclose(fp);

    fp = fopen("/sys/class/net/" INTERFACE "/statistics/tx_bytes", "r");
    if (fp == NULL) {
        perror("Error opening tx_bytes");
        return -1;
    }
    fscanf(fp, "%lu", &tx);
    fclose(fp);

    *rx_bytes = rx;
    *tx_bytes = tx;

    return 0;
}

// Function to calculate speed in Mbps
double calculate_speed(unsigned long prev, unsigned long curr, double interval_sec) {
    // Calculate the difference in bytes and convert to megabits (1 byte = 8 bits)
    double diff = curr - prev;
    return (diff * 8) / (1024 * 1024) / interval_sec;  // Mbps
}

int main() {
    unsigned long prev_rx_bytes = 0, prev_tx_bytes = 0;
    unsigned long current_rx_bytes, current_tx_bytes;
    double rx_speed, tx_speed;
    double interval = 1.0;  // Interval to measure speed (in seconds)

    while (1) {
        // Get current statistics for rx_bytes and tx_bytes
        if (get_network_stats(&current_rx_bytes, &current_tx_bytes) != 0) {
            printf("Failed to get network stats\n");
            break;
        }

        // Calculate the speeds (difference in bytes over time)
        rx_speed = calculate_speed(prev_rx_bytes, current_rx_bytes, interval);
        tx_speed = calculate_speed(prev_tx_bytes, current_tx_bytes, interval);

        // Print the speeds
        printf("Incoming speed: %.2f Mbps | Outgoing speed: %.2f Mbps\n", rx_speed, tx_speed);

        // Update the previous byte counters
        prev_rx_bytes = current_rx_bytes;
        prev_tx_bytes = current_tx_bytes;

        // Wait for the next interval
        sleep(1);  // Check every second
    }

    return 0;
}

