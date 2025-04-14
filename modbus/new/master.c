#include <stdio.h>
#include <stdlib.h>
#include <modbus/modbus.h>
#include <errno.h>


#define SERVER_IP "192.168.0.133"
#define SERVER_PORT 1502

int main() {
    modbus_t *ctx;
    uint16_t register_value;
    int register_address, rc;

    // Initialize the master
    ctx = modbus_new_tcp(SERVER_IP, SERVER_PORT);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the Modbus context\n");
        return -1;
    }

       // Set the response timeout to 2 seconds and 500 milliseconds (500,000 microseconds)
    modbus_set_response_timeout(ctx, 10, 500000);


    // Connect to the slave
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Unable to connect: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    printf("Connected to the slave.\n");

    while (1) {
        // Get the register address from the user
        printf("Enter the register address (1 - , or -1 to quit): ");
        scanf("%d", &register_address);

        if (register_address == -1) {
            printf("Exiting...\n");
            break;
        }

        if (register_address < 0 || register_address >= 100) {
            printf("Invalid register address. Try again.\n");
            continue;
        }

        // Read the register value from the slave
        rc = modbus_read_registers(ctx, register_address, 1, &register_value);
        if (rc == -1) {
            fprintf(stderr, "Failed to read: %s\n", modbus_strerror(errno));
        } else {
            printf("Value at register %d: %d\n", register_address, register_value);
        }
    }

    // Clean up
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}

