#include <stdio.h>
#include <stdlib.h>
#include <modbus/modbus.h>
#include <errno.h>

#define SERVER_IP "192.168.0.20"
#define SERVER_PORT 1502

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


int is_valid_register(int reg_address) {
    for (int i = 0; i < sizeof(valid_register_addresses) / sizeof(valid_register_addresses[0]); i++) {
        if (valid_register_addresses[i] == reg_address) {
            return 1;  // Valid address
        }
    }
    return 0;  // Invalid address
}


int main() {
    modbus_t *ctx;
    uint16_t register_values[4];  // Array to hold 4 registers' values
    int register_address, rc ,i,z;

    // Initialize the master
    ctx = modbus_new_tcp(SERVER_IP, SERVER_PORT);
    if (ctx == NULL) {
        fprintf(stderr, "Unable to create the Modbus context\n");
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 10;      // 2 seconds
    timeout.tv_usec = 500000; // 500 milliseconds
    modbus_set_response_timeout(ctx, &timeout); // Pass the address of the struct

    // Connect to the slave
    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Unable to connect: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    printf("Connected to the slave.\n");

     while (1) 
     {
        // Get the register address from the user
        printf("Enter the register address (e.g. 300001, 300003, ..., or -1 to quit): ");
        scanf("%d", &register_address);

        if (register_address == -1) {
            printf("Exiting...\n");
            break;
        }

        if (!is_valid_register(register_address)) {
            printf("Invalid register address. Valid addresses are:\n");
            for (i = 0; i < sizeof(valid_register_addresses) / sizeof(valid_register_addresses[0]); i++) {
                printf("300%03d ", valid_register_addresses[i] % 1000);  // Print a subset of valid addresses for clarity
            }
            printf("\nTry again. %d\n",i);
            continue;
        }

        for(z=0; z < sizeof(valid_register_addresses) / sizeof(valid_register_addresses[0]); z++) 
	{
		if(valid_register_addresses[z] == register_address)
	                		break;
	}


        // Read 4 registers starting from the given register address
        rc = modbus_read_registers(ctx, z , 4, register_values);
        if (rc == -1) {
            fprintf(stderr, "Failed to read: %s\n", modbus_strerror(errno));
        } else {
            printf("Values at register %d: ", register_address);
            for (int i = 0; i < 4; i++) {
                printf("0x%x ", register_values[i]);
            }
            printf("\n");
        }
    }
    // Clean up
    modbus_close(ctx);
    modbus_free(ctx);
    return 0;
}

