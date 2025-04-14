#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_ID     1
#define MODBUS_PORT   1502

int registers[500] = {0};

// Process the request and send the response dynamically
void process_query(modbus_t *mb, modbus_mapping_t *mb_mapping, uint8_t *query) {
    // The query will tell us which register to modify. We handle it based on the register address.
    uint16_t starting_address = (query[2] << 8) | query[3]; // Query address (e.g. 300001)
    uint16_t num_registers = (query[4] << 8) | query[5];    // Number of registers requested

    printf("Received Query: Start Address: %u, Num Registers: %u\n", starting_address, num_registers);

    // Loop through the requested range of registers and dynamically assign values
    for (int i = 0; i < num_registers; i++) {
        uint16_t current_address = starting_address + i;

        // Dynamically assign values to the registers based on the starting address
        if (current_address >= 300001 && current_address <= 300323) {
            int index = current_address - 300001;
            if (index % 2 == 0) {
                registers[index] = (index / 2 + 1) * 100;  // Values like 100, 200, 300, etc.
            } else {
                registers[index] = ((index - 1) / 2 + 1) * 100;  // For odd indices, use the previous even value
            }
        }

        // Update the register map with the new value
        mb_mapping->tab_registers[i] = registers[starting_address - 300001 + i];
    }

    // Now send the response with the updated register values
    modbus_reply(mb, query, num_registers * 2 + 6, mb_mapping);
}

void initialize_registers() {
    // Initialize some registers with default values (optional, to be used later in the callback function)
    for (int i = 0; i < 500; i++) {
        registers[i] = (i / 2 + 1) * 100;  // Fill initial values like 100, 200, 300, etc.
    }
}

// Modbus slave function to respond to read requests
void start_slave() {
    modbus_t *mb;       // Modbus context
    modbus_mapping_t *mb_mapping;  // Modbus mapping for registers
    int s;              // Socket for the slave server

    // Initialize the Modbus slave
    mb = modbus_new_tcp(NULL, MODBUS_PORT); // Listen on all IP addresses on port 1502
    if (mb == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        exit(1);
    }

    // Initialize the register map
    mb_mapping = modbus_mapping_new(0, 0, 500, 0);  // 500 registers (300001-300500)
    if (mb_mapping == NULL) {
        fprintf(stderr, "Unable to allocate memory for the mapping\n");
        modbus_free(mb);
        exit(1);
    }

    // Set predefined values in the register map
    initialize_registers();

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
            // Process the request dynamically and send the response
            process_query(mb, mb_mapping, query);
        }
    }

    // Cleanup
    modbus_mapping_free(mb_mapping);
    modbus_free(mb);
}

int main() {
    initialize_registers();
    start_slave();
    return 0;
}

