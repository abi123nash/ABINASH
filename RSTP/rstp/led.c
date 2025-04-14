#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

// GPIO pins to control
int PINS[] = {122, 123, 124};
int num_pins = 3;

// Function to export a GPIO pin
void export_gpio(int pin) {
    FILE *fp = fopen("/sys/class/gpio/export", "w");
    if (fp == NULL) {
        perror("Failed to export GPIO pin");
        exit(1);
    }
    fprintf(fp, "%d", pin);
    fclose(fp);
}

// Function to set a GPIO pin direction
void set_gpio_direction(int pin, const char *direction) {
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("Failed to set GPIO direction");
        exit(1);
    }
    fprintf(fp, "%s", direction);
    fclose(fp);
}

// Function to write value to a GPIO pin
void write_gpio_value(int pin, int value) {
    char path[35];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    FILE *fp = fopen(path, "w");
    if (fp == NULL) {
        perror("Failed to write GPIO value");
        exit(1);
    }
    fprintf(fp, "%d", value);
    fclose(fp);
}

// Function to unexport a GPIO pin
void unexport_gpio(int pin) {
    FILE *fp = fopen("/sys/class/gpio/unexport", "w");
    if (fp == NULL) {
        perror("Failed to unexport GPIO pin");
        exit(1);
    }
    fprintf(fp, "%d", pin);
    fclose(fp);
}

// Cleanup function to handle GPIO cleanup on exit
void cleanup(int signum) {
    for (int i = 0; i < num_pins; i++) {
        write_gpio_value(PINS[i], 0);  // Set pin value to 0 (off)
        unexport_gpio(PINS[i]);        // Unexport the GPIO pin
    }
    printf("GPIO cleanup complete.\n");
    exit(0);
}

int main() {
    // Set up SIGINT (Ctrl+C) signal handler for cleanup
    signal(SIGINT, cleanup);

    // Export the GPIO pins and set them as outputs
    for (int i = 0; i < num_pins; i++) {
        export_gpio(PINS[i]);
        set_gpio_direction(PINS[i], "out");
    }

    printf("Starting LED blink test. Press Ctrl+C to stop.\n");

    // Blink the LEDs
    while (1) {
        // Turn LEDs on
        for (int i = 0; i < num_pins; i++) {
            write_gpio_value(PINS[i], 1);
        }
        usleep(500000);  // Sleep for 0.5 seconds

        // Turn LEDs off
        for (int i = 0; i < num_pins; i++) {
            write_gpio_value(PINS[i], 0);
        usleep(500000);  // Sleep for 0.5 seconds
        }
        usleep(500000);  // Sleep for 0.5 seconds
    }

    return 0;
}

