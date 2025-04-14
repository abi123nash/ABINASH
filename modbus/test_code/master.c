#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define SERVER_IP "192.168.0.141"
#define PORT 1502

int main() {
    modbus_t *ctx;
    uint16_t tab_reg[32];
    int rc;

    // Create a Modbus TCP context
    ctx = modbus_new_tcp(SERVER_IP, PORT);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }

    // Connect to the server
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }
    printf("Connected to Modbus TCP server at %s:%d\n", SERVER_IP, PORT);

    // Read 10 Holding Registers starting from address 0
    rc = modbus_read_registers(ctx, 0, 10, tab_reg);
    if (rc == -1) {
        fprintf(stderr, "Read failed: %s\n", modbus_strerror(errno));
    } else {
        printf("Read %d registers:\n", rc);
        for (int i = 0; i < rc; i++) {
            printf("Register[%d] = %d\n", i, tab_reg[i]);
        }
    }

    // Write a single register at address 0 with value 12345
    rc = modbus_write_register(ctx, 0, 12345);
    if (rc == -1) {
        fprintf(stderr, "Write failed: %s\n", modbus_strerror(errno));
    } else {
        printf("Wrote value 12345 to register 0\n");
    }

    // Disconnect and clean up
    modbus_close(ctx);
    modbus_free(ctx);

    return 0;
}

