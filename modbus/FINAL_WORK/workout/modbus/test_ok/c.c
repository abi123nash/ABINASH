#include <stdio.h>
#include <stdlib.h>
#include <modbus/modbus.h>
#include <errno.h>
#include <unistd.h>

#define SERVER_IP "192.168.0.153"  // Change to match server's IP
#define SERVER_PORT 502
#define MODULE_ID_START 1000  // First CAN module register
#define REGISTER_COUNT 28     // 56 bytes = 28 registers

void print_received_data(uint16_t *data) {
    printf("Received CAN Data (HEX):\n");
    for (int i = 0; i < REGISTER_COUNT; i++) {
        printf("%04X ", data[i]);
        if ((i + 1) % 8 == 0) printf("\n");
    }
    printf("\n");
}

int main() {
    modbus_t *ctx;
    uint16_t tab_registers[REGISTER_COUNT];
    uint8_t re_connect=0;

    while (1) 
    {
        /*
	 *reset flag
         */
        re_connect=0;
	    
        ctx = modbus_new_tcp(SERVER_IP, SERVER_PORT);
        if (!ctx) {
            fprintf(stderr, "Failed to create Modbus context: %s\n", modbus_strerror(errno));
            return -1;
        }
          

         struct timeval timeout;
         timeout.tv_sec = 10;      // 2 seconds
         timeout.tv_usec = 500000; // 500 milliseconds
         modbus_set_response_timeout(ctx, &timeout); // Pass the address of the struct


        printf("Connecting to server at %s:%d...\n", SERVER_IP, SERVER_PORT);

        if (modbus_connect(ctx) == -1) {
            fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
            modbus_free(ctx);
            sleep(1);  // Retry after 5 seconds
            continue;
        }

        printf("Connected to server.\n");

        while (1) 
	{
            for (int module = 0; module < 5; module++) 
	    {
                int register_address = MODULE_ID_START + (module * REGISTER_COUNT);
                printf("\nRequesting data from Module ID: %d\n", register_address);

                int rc = modbus_read_registers(ctx, register_address, REGISTER_COUNT, tab_registers);
                if (rc == -1) 
		{
                    fprintf(stderr, "Failed to read module %d: %s\n", register_address, modbus_strerror(errno));
                    re_connect=1;  // If an error occurs, assume disconnection
		    break;     
                } 
		else 
		{
                    print_received_data(tab_registers);
                }
            }

	    if(re_connect)
	     {
		break;     
	     }	     

            sleep(2);  // Delay before next read cycle
        }

        printf("Server disconnected. Reconnecting...\n");
        modbus_close(ctx);
        modbus_free(ctx);
        sleep(1);  // Wait before reconnecting
    }

    return 0;
}

