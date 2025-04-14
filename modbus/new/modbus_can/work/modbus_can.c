#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>

#define SERVER_ID     1
#define MODBUS_PORT   1502
#define MAX_DATA_LEN 8
#define CAN_INTERFACE  "can0"
#define CAN_BITRATE    125000


size_t register_count = 0;


/*
 *Predefined register addresses
 */

uint32_t valid_register_addresses[] = 
{
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



// Print error messages and exit
void print_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Setup CAN interface
void setup_can_interface(const char *interface, int bitrate) {
    char command[128];

    snprintf(command, sizeof(command), "ip link set %s down", interface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    snprintf(command, sizeof(command), "ip link set %s up type can bitrate %d", interface, bitrate);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    printf("Successfully set up CAN interface %s with bitrate %d\n", interface, bitrate);

       // Set up the CAN interface with the specified bitrate
    snprintf(command, sizeof(command), "ip link set %s up", interface);
    if (system(command) != 0) {
        fprintf(stderr, "Failed to set up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    printf("Successfully %s ip link set up\n", interface);

}

// Send a CAN message
void send_can_message(int socket_fd, struct can_frame *frame) {
    if (write(socket_fd, frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        print_error("Error sending CAN frame");
    }
    printf("CAN frame sent: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");
}

// Receive a CAN message
void receive_can_message(int socket_fd, struct can_frame *frame) {
    ssize_t nbytes;
    nbytes = read(socket_fd, frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        print_error("Error receiving CAN frame");
    }
    printf("Received CAN frame: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");
}

// Modbus slave to handle requests
void start_slave() 
{
    modbus_t *mb;
    modbus_mapping_t *mb_mapping;
    int s, socket_fd;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    // Define a static CAN frame
    static struct can_frame frame_send = {
    .can_id = 0x143,  // CAN ID
    .can_dlc = 8,     // Data Length Code
    .data = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44} // Data bytes
    };

    // Initialize Modbus context and mapping
    mb = modbus_new_tcp(NULL, MODBUS_PORT); 
    if (mb == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        exit(1);
    }

    mb_mapping = modbus_mapping_new(0, 0, 1000, 0);  // 1000 registers
    if (mb_mapping == NULL) {
        fprintf(stderr, "Unable to allocate memory for the mapping\n");
        modbus_free(mb);
        exit(1);
    }

    // Setup CAN interface
    setup_can_interface(CAN_INTERFACE, CAN_BITRATE);

    // Create a socket for CAN communication
    socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd < 0) {
        print_error("Socket creation failed");
    }

    strcpy(ifr.ifr_name, CAN_INTERFACE);
    if (ioctl(socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        print_error("Error getting CAN interface index");
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // Bind the socket to the CAN interface
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        print_error("Error binding socket to CAN interface");
    }

    // Open Modbus TCP server
    s = modbus_tcp_listen(mb, 1);
    if (s == -1) {
        fprintf(stderr, "Unable to open Modbus TCP server\n");
        modbus_mapping_free(mb_mapping);
        modbus_free(mb);
        exit(1);
    }

    modbus_tcp_accept(mb, &s);
    printf("Modbus slave started and listening...\n");

    while (1) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int rc;

        // Receive the Modbus request from the master
        rc = modbus_receive(mb, query);
        if (rc > 0) 
	{
            printf("Received query:\n");
            for (int i = 0; i < rc; i++) 
	    {
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

            if (reg_data != NULL) 
	    {
                // Map the data to Modbus registers
                for (int i = 0; i < 8; i++) 
		{
                     frame_send.data[i] = reg_data->data[i];
		}

            // Send data on CAN bus
            send_can_message(socket_fd, &frame_send);  // Send CAN frame before Modbus reply


            // Wait for CAN response
            receive_can_message(socket_fd, &frame); // Receive CAN frame



	     // Process the received CAN data and map it to the Modbus registers
            // Example: Assuming that the first 4 bytes of the CAN data map to the first 2 Modbus registers
            if (frame.can_dlc >= 8) 
	    {
                // Map the first two 16-bit values from CAN data to Modbus registers
                
		      // Map the data to Modbus registers
                for (int i = 0; i < 4; i++) 
		{
                    mb_mapping->tab_registers[i] = (frame.data[i*2] << 8) | frame.data[i*2+1];
                }
		    printf("Mapped CAN data to Modbus registers: 0x%X, 0x%X, 0x%X, 0x%X\n",
                         mb_mapping->tab_registers[0], mb_mapping->tab_registers[1]
		        ,mb_mapping->tab_registers[2],mb_mapping->tab_registers[3]);
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
             }
	    else 
	    {
                // Address not found, send an error response
                printf("Register address %d not found.\n", starting_address);
                // You could add error handling logic here (e.g., exception response)
	    } 

    
         }
	else
	{
	   printf("modbus recive error: ! \n");	
	}
  
   }	 

    // Cleanup
    modbus_mapping_free(mb_mapping);
    modbus_free(mb);
    close(socket_fd);
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
	free_registers();

    return 0;
}

