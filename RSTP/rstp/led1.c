#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <termios.h>
#include <fcntl.h>

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
    printf("\nCleaning up GPIOs...\n");
    for (int i = 122; i <= 124; i++) {
        write_gpio_value(i, 0);  // Set pin value to 0 (off)
        unexport_gpio(i);        // Unexport the GPIO pin
    }
    printf("GPIO cleanup complete.\n");
    exit(0);
}

// Function to show the application menu
void show_menu() {
    printf("\nLED Control Application\n");
    printf("1. Blink LED on Pin 122\n");
    printf("2. Blink LED on Pin 123\n");
    printf("3. Blink LED on Pin 124\n");
    printf("4. Exit\n");
    printf("Please select an option (1-4): ");
}

// Function to get a valid GPIO pin number from the user
int get_user_input_pin() {
    int pin;
    while (1) {
        scanf("%d", &pin);
        if (pin >= 122 && pin <= 124) {
            return pin;
        } else {
            printf("Your input is wrong. Please select a valid pin (122-124): ");
        }
    }
}

// Function to get a valid LED state from the user (ON/OFF)
int get_user_input_led_state() {
    char state[10];
    while (1) {
        scanf("%s", state);
        
        // Convert the input to uppercase for easy comparison
        for (int i = 0; state[i]; i++) {
            state[i] = toupper(state[i]);
        }

        if (strcmp(state, "ON") == 0) {
            return 1; // Turn LED ON
        } else if (strcmp(state, "OFF") == 0) {
            return 0; // Turn LED OFF
        } else {
            printf("Your input is wrong. Please enter 'ON' or 'OFF': ");
        }
    }
}

// Function to check if F1 key is pressed
int check_f1_key() {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK); // Set non-blocking mode

    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf); // Reset to blocking mode

    if (ch == 27) {  // Escape character
        ch = getchar();
        if (ch == 79) {  // Arrow keys
            ch = getchar();
            if (ch == 80) {  // F1 key
                return 1;
            }
        }
    }
    return 0;
}

int main() {
    // Set up SIGINT (Ctrl+C) signal handler for cleanup
    signal(SIGINT, cleanup);

    int pin_to_control;
    int led_state;
    
    while (1) {
        if (check_f1_key()) {
            show_menu();
            pin_to_control = get_user_input_pin();

            if (pin_to_control == 4) {
                printf("Exiting the application...\n");
                cleanup(0); // Cleanup GPIO before exiting
            }

            // Export the selected GPIO pin and set it as an output
            export_gpio(pin_to_control);
            set_gpio_direction(pin_to_control, "out");

            // Ask the user whether to turn the LED on or off
            printf("Blink LED on Pin %d (ON/OFF): ", pin_to_control);
            led_state = get_user_input_led_state();

            // Set the LED state
            write_gpio_value(pin_to_control, led_state);

            printf("LED on Pin %d is now %s.\n", pin_to_control, (led_state == 1) ? "ON" : "OFF");
        }

        // Print continuous status every second
        printf("Status: Press F1 to control LED...\n");
        sleep(1);
    }

    return 0;
}

