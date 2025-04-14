#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <modbus/modbus.h>
#include <errno.h>
#include <unistd.h>

#define SERVER_PORT 502
#define MODULE_COUNT 5
#define REGISTER_PER_MODULE 28
#define REGISTER_START 1000

uint8_t can_data[MODULE_COUNT][REGISTER_PER_MODULE * 2];

void initialize_can_data() {
    for (int module = 0; module < MODULE_COUNT; module++) {
        for (int i = 0; i < REGISTER_PER_MODULE * 2; i++) {
            can_data[module][i] = (module + 1) * 0x10 + i;
        }
    }
}

void map_can_data_to_modbus(modbus_mapping_t *mb_mapping) {
    for (int module_index = 0; module_index < MODULE_COUNT; module_index++) {
        int base_register = REGISTER_START + (module_index * REGISTER_PER_MODULE);
        for (int i = 0; i < REGISTER_PER_MODULE; i++) {
            mb_mapping->tab_registers[base_register + i] =
                (can_data[module_index][i * 2] << 8) | can_data[module_index][i * 2 + 1];
        }
    }
}

int main() {
    modbus_t *ctx;
    modbus_mapping_t *mb_mapping;
    int server_socket;
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];

    ctx = modbus_new_tcp("0.0.0.0", SERVER_PORT);
    if (!ctx) {
        fprintf(stderr, "Error: Failed to create Modbus context: %s\n", modbus_strerror(errno));
        return -1;
    }

    modbus_set_debug(ctx, 0);  // Enable debug mode

    mb_mapping = modbus_mapping_new(0, 0, REGISTER_START + (MODULE_COUNT * REGISTER_PER_MODULE), 0);
    if (!mb_mapping) {
        fprintf(stderr, "Error: Failed to allocate Modbus registers: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    initialize_can_data();
    map_can_data_to_modbus(mb_mapping);

    server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket == -1) {
        fprintf(stderr, "Error: Failed to listen on Modbus TCP: %s\n", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return -1;
    }

    printf("Modbus TCP Server started on port %d, waiting for clients...\n", SERVER_PORT);

    while (1) 
    {    
        int client_socket = modbus_tcp_accept(ctx, &server_socket);
        if (client_socket == -1) {
            fprintf(stderr, "Error: Failed to accept client: %s\n", modbus_strerror(errno));
            continue;
        }

        printf("Client connected.\n");

        while (1) 
	{
            int rc = modbus_receive(ctx, query);
            if (rc > 0) 
	    {
                modbus_reply(ctx, query, rc, mb_mapping);
            }
	    else if (rc == -1) 
	    {
                printf("Client disconnected. Waiting for next connection...\n");
		close(client_socket);
                break;
            }
        }

        close(client_socket);
    }

    close(server_socket);
    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);
    return 0;
}

