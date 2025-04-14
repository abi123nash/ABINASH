#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define SERVER_ID     1
#define MODBUS_PORT   1502

void start_slave() {
    modbus_t *mb;                    // Modbus context
    modbus_mapping_t *mb_mapping;    // Modbus mapping for registers
    int s;                           // Socket for the slave server

    // Example data array to be sent to the master
    int data[] = {0x1f, 0x2f, 0x3f, 0x4f, 0x5f, 0x6f, 0x7f, 0x8f};

    // Initialize the Modbus slave
    mb = modbus_new_tcp(NULL, MODBUS_PORT); // Listen on all IP addresses on port 1502
    if (mb == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        exit(1);
    }

    // Initialize the register map
    mb_mapping = modbus_mapping_new(0, 0, 143, 0); // 100 registers
    if (mb_mapping == NULL) {
        fprintf(stderr, "Unable to allocate memory for the mapping\n");
        modbus_free(mb);
        exit(1);
    }

    // Open the Modbus TCP server
    s = modbus_tcp_listen(mb, 1);
    if (s == -1) {
        fprintf(stderr, "Unable to open Modbus TCP server\n");
        modbus_mapping_free(mb_mapping);
        modbus_free(mb);
        exit(1);
    }

    // Accept incoming connections
    modbus_tcp_accept(mb, &s);
    printf("Slave started and listening...\n");

    while (1) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int rc;

        // Receive the request from the master
        rc = modbus_receive(mb, query);

        if (rc > 0) {
            printf("Received query:\n");
            for (int i = 0; i < rc; i++) {
                printf("0x%02X ", query[i]);
            }
            printf("\n");

            // Parse the function code and starting address
            uint8_t function_code = query[7];
            uint16_t starting_address = (query[8] << 8) | query[9];
            uint16_t quantity = (query[10] << 8) | query[11];

            printf("Function code: %d\n", function_code);
            printf("Starting address: %d\n", starting_address);
            printf("Quantity: %d\n", quantity);


	    query[9] = 0;

            // Populate response registers
            for (int i = 0; i < 4; i++) {
                mb_mapping->tab_registers[i] = (data[i * 2] << 8) | data[i * 2 + 1];
            }

            // Now respond to the Modbus master
            modbus_reply(mb, query, rc, mb_mapping);
        }
    }

    // Cleanup
    modbus_mapping_free(mb_mapping);
    modbus_free(mb);
}

int main() {
    start_slave();
    return 0;
}

