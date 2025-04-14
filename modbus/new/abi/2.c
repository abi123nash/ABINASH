#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define SERVER_ID     1
#define MODBUS_PORT   1502

size_t register_count = 0;

// Predefined register addresses
int valid_register_addresses[] = {
    300001, 300003, 300005, 300007, 300009, 300011, 300013, 300015,
    300017, 300019, 300021, 300023, 300025, 300027, 300029, 300033,
    300037, 300041, 300045, 300049, 300053, 300057, 300059, 300061,
    300063, 300065, 300067, 300069, 300071, 300075, 300079, 300083,
    300087, 300091, 300095, 300099, 300101, 300102, 300103, 300104,
    300105, 300106, 300107, 300108, 300109, 300110, 300111, 300112,
    300113, 300115, 300117, 300119, 300121, 300123, 300125, 300127,
    300129, 300133, 300137, 300141, 300145, 300147, 300149, 300151,
    300153, 300155, 300157, 300159, 300161, 300163, 300165, 300167,
    300169, 300171, 300173, 300177, 300181, 300185, 300189, 300193,
    300197, 300201, 300203, 300205, 300207, 300209, 300211, 300213,
    300215, 300219, 300223, 300227, 300231, 300235, 300239, 300243,
    300245, 300247, 300248, 300249, 300250, 300251, 300252, 300253,
    300254, 300255, 300256, 300257, 300258, 300259, 300260, 300261,
    300262, 300263, 300264, 300265, 300267, 300269, 300271, 300273,
    300275, 300277, 300279, 300281, 300283, 300285, 300287, 300289,
    300291, 300293, 300295, 300297, 300299, 300301, 300303, 300305,
    300307, 300309, 300311, 300313, 300315, 300319, 300323
};


// Struct to store register address and its corresponding data
typedef struct {
    uint32_t address;  // Register address (e.g., 300001, 300003, ...)
    uint16_t data[8];  // 8 register data for this address
} RegisterData;

// Register array dynamically created
RegisterData *registers = NULL;

void init_registers(size_t count) {
    // Initialize the registers dynamically
    registers = (RegisterData*) malloc(sizeof(RegisterData) * count);
    if (registers == NULL) {
        perror("Failed to allocate memory for registers");
        exit(EXIT_FAILURE);
    }

    // Initialize register data (this is just an example, modify as needed)
    for (size_t i = 0; i < count; i++) {
        registers[i].address = valid_register_addresses[i];
        for (int j = 0; j < 8; j++) {
            registers[i].data[j] = (uint16_t)(0x10 + i);  // Just filling data for now
        }
    }
}


// Function to find the corresponding data for a given address
RegisterData* get_register_data(uint32_t address) {
    for (int i = 0; i < register_count ; i++) {
        if (registers[i].address == address) {
            return &registers[i];
        }
    }
    return NULL;  // Return NULL if address not found
}

int start_slave() {
    modbus_t *mb;                    // Modbus context
    modbus_mapping_t *mb_mapping;    // Modbus mapping for registers
    int s;                           // Socket for the slave server
    
    // Initialize the Modbus slave
    mb = modbus_new_tcp(NULL, MODBUS_PORT); // Listen on all IP addresses on port 1502
    if (mb == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        exit(1);
    }

    // Initialize the register map
   // mb_mapping = modbus_mapping_new(0, 0, 143, 0); // 143 registers available
    mb_mapping = modbus_mapping_new(0, 0, 500, 0); // Adjust size as needed
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
            uint64_t starting_address = (query[8] << 8) | query[9];
            uint16_t quantity = (query[10] << 8) | query[11];

            printf("Function code: %d\n", function_code);
            printf("Starting address: %d\n", starting_address);
            printf("Quantity: %d\n", quantity);

           for(int z=0;z < register_count ; z++)
	   {
		   if(z == starting_address )
	             {		   
	             starting_address = valid_register_addresses[z];
		     break;
		     }
	   }

            printf("Read address  %d \n",starting_address);

            // Find the register data based on the starting address
            RegisterData *reg_data = get_register_data(starting_address);

            if (reg_data != NULL) {
                // Map the data to Modbus registers
                for (int i = 0; i < 4; i++) {
                    mb_mapping->tab_registers[i] = (reg_data->data[i*2] << 8) | reg_data->data[i*2+1];
                }

		query[9] = 0;

                // Send the response to the master
		// Now respond to the Modbus master
                int reply_status = modbus_reply(mb, query, rc, mb_mapping);

                if (reply_status == -1) {
                    // Log error message or handle error
                    fprintf(stderr, "Error in modbus_reply: %s\n", modbus_strerror(errno));
                    // You can also return or exit, depending on how you want to handle this
                  return -1; // Or any appropriate error code
               } 

                printf("Responded with data for register address %d: ", starting_address);
                for (int i = 0; i < 8; i++) {
                    printf("0x%04x ", reg_data->data[i]);
                }
                printf("\n");
            } else {
                // Address not found, send an error response
                printf("Register address %d not found.\n", starting_address);
                // You could add error handling logic here (e.g., exception response)
            }
        }
    }

    // Cleanup
    modbus_mapping_free(mb_mapping);
    modbus_free(mb);
}


void free_registers() {
    free(registers);
    registers = NULL;
}


int main() {
       
    register_count = sizeof(valid_register_addresses) / sizeof(valid_register_addresses[0]);
        // Initialize 100 registers (example size, you can modify this)
    init_registers(register_count);
    start_slave();
    // Free dynamically allocated memory for registers
    free_registers();

    return 0;
}

