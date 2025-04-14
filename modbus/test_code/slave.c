#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define PORT 1502

int main() {
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;
    int server_socket;
    
    // Create a Modbus TCP context
    ctx = modbus_new_tcp("0.0.0.0", PORT);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return -1;
    }

    // Allocate a Modbus mapping (Holds data for registers)
    mb_mapping = modbus_mapping_new(0, 0, 100, 100);
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate mapping: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    // Bind the server socket
    server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket == -1) {
        fprintf(stderr, "Unable to listen on port: %s\n", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return -1;
    }
    printf("Modbus TCP Server running on port %d...\n", PORT);

    while (1) {
        // Accept a connection from a Modbus TCP master
        int client_socket = modbus_tcp_accept(ctx, &server_socket);
        if (client_socket == -1) {
            fprintf(stderr, "Connection error: %s\n", modbus_strerror(errno));
            continue;
        }

        // Handle requests
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int rc = modbus_receive(ctx, query);
        if (rc > 0) {
            modbus_reply(ctx, query, rc, mb_mapping);
        } else if (rc == -1) {
            fprintf(stderr, "Connection closed or error: %s\n", modbus_strerror(errno));
        }

        close(client_socket);
    }

    // Clean up
    close(server_socket);
    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);

    return 0;
}

