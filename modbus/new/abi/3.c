#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define SERVER_ID     1
#define MODBUS_PORT   1502

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


size_t register_count = 0;

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

void free_registers() {
    free(registers);
    registers = NULL;
}

int get_register_data(uint32_t address, uint16_t *output) {
    for (size_t i = 0; i < register_count; i++) {
        if (registers[i].address == address) {
            // Copy the data of this register to the output
            for (int j = 0; j < 8; j++) {
                output[j] = registers[i].data[j];
            }
            return 0;  // Found and copied data
        }
    }
    return -1;  // Address not found
}

int main() {

    register_count = sizeof(valid_register_addresses) / sizeof(valid_register_addresses[0]);
    

	// Initialize 100 registers (example size, you can modify this)
    init_registers(register_count);

    uint16_t output[8];
    uint32_t test_address = 300003;

    // Test get_register_data function
    if (get_register_data(test_address, output) == 0) {
        printf("Data for address %u: ", test_address);
        for (int i = 0; i < 8; i++) {
            printf("0x%X ", output[i]);
        }
        printf("\n");
    } else {
        printf("Register with address %u not found.\n", test_address);
    }

    // Free dynamically allocated memory for registers
    free_registers();

    return 0;
}

