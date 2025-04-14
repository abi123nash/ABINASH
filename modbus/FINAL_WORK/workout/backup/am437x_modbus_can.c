#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/socket.h>


#define CAN_INTERFACE "can0"
#define CAN_BITRATE    125000

#define CAN_REQUEST_ID  0x01200333
#define CAN_RESPONSE_ID 0x01210333
#define CAN_ACK_ID      0x01290333
#define Heartbeat_ID    0x017E0333

/*
 *Polynomial for CRC-16
 */
#define CRC_POLY 0xA001

#define FRAME_DATA_SIZE 5
#define TOTAL_FRAMES    12

typedef struct __attribute__((packed)) {
    uint16_t data1;
    uint16_t data2;
    uint16_t data3;
    uint16_t data4;
    uint16_t data5;
    uint16_t data6;
    uint16_t data7;
    uint16_t data8;
    uint16_t data9;
    uint16_t data10;
    uint16_t data11;
    uint16_t data12;
    uint16_t data13;
    uint16_t data14;
    uint16_t data15;
    uint16_t data16;
    uint16_t data17;
    uint16_t data18;
    uint16_t data19;
    uint16_t data20;
    uint16_t data21;
    uint16_t data22;
    uint16_t data23;
    uint32_t data24;
    uint32_t data25;
    uint16_t data26;
} BR_DATA;


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


// Function to send a CAN message
int send_can_message(int socket_fd, struct can_frame *frame) {
    if (write(socket_fd, frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        print_error("Error sending CAN frame");
	return -1;
    }
    printf("CAN frame sent: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");
    
    return 0;
}

// Receive a CAN message
int receive_can_message(int socket_fd, struct can_frame *frame) {
    ssize_t nbytes;
    nbytes = read(socket_fd, frame, sizeof(struct can_frame));
    if (nbytes < 0) {
        print_error("Error receiving CAN frame");
	return -1;
    }
    printf("Received CAN frame: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");
	
    return 0;
}


// Thread function for heartbeat message
void *heartbeat_thread(void *arg) {
    int socket_fd = *(int *)arg;  // Get socket from main
    uint32_t uptime = 0;
    struct can_frame heartbeat_msg;
    
    heartbeat_msg.can_id = Heartbeat_ID;  // CAN ID for heartbeat
    heartbeat_msg.can_dlc = 8;          // Data length

    printf("Heartbeat: This CAN frame is sent every 500ms.\n");
   
    while (1) {
        // Set the uptime value in the first 4 bytes
        heartbeat_msg.data[0] = (uint8_t)(uptime >> 24);
        heartbeat_msg.data[1] = (uint8_t)(uptime >> 16);
        heartbeat_msg.data[2] = (uint8_t)(uptime >> 8);
        heartbeat_msg.data[3] = (uint8_t)uptime;

        // Set remaining bytes to 0
        memset(&heartbeat_msg.data[4], 0, 4);

        // Send the heartbeat message
        send_can_message(socket_fd, &heartbeat_msg);

        // Increment uptime and sleep for 500ms
        uptime++;
        usleep(500000);
    }
    
    return NULL;
}

uint16_t calculate_crc(uint8_t *data, int length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

int receive_can_response(int socket_fd, BR_DATA *br_data) {
    struct can_frame frame;
    uint8_t received_data[56] = {0};
    int received_bytes = 0;
    uint8_t frame_index;
    int expected_frames = 12;
    int ret=0;

    for (int i = 0; i < expected_frames; i++) {
        
	printf("ETU to TCP: Data Read Response received.\n");
        ret = receive_can_message(socket_fd, &frame); // Receive CAN frame
        if (0 != ret) {
            perror("CAN response receive failed");
            return -1;
        }


        frame_index = frame.data[0] -1;

        uint16_t crc_received = (frame.data[6] << 8) | frame.data[7];

        if (calculate_crc(frame.data, 6) != crc_received) {
            printf("CRC Error on frame %d\n", frame_index);
            return -1;
        }

        memcpy(&received_data[frame_index * 5], &frame.data[1], 5);

        struct can_frame ack_frame;
        ack_frame.can_id = CAN_ACK_ID + (frame_index * 5);
        ack_frame.can_dlc = 8;
        memset(ack_frame.data, 0, 8);
        ack_frame.data[0] = frame_index + 1;
        ack_frame.data[6] = 0xFF;
        ack_frame.data[7] = 0xFE - frame_index;
        
	printf("TCP to ETU: Data Read Acknowledgment sent.\n\n");
        ret = send_can_message(socket_fd, &ack_frame);
        if (0 != ret) {
            perror("CAN ACK send failed");
            return -1;
        }

    }
    
    printf("Reception complete: All fragmented data has been successfully reassembled.\n");
    memcpy(br_data, received_data, sizeof(BR_DATA));
    return 0;
}

void print_br_data(const BR_DATA *data) {
    const uint16_t *ptr = (const uint16_t *)data;
    
    for (int i = 0; i < 23; i++) {
        printf("data%d: 0x%04X\n", i + 1, ptr[i]);
    }

    // Print 32-bit values separately
    printf("data24: 0x%08X\n", data->data24);
    printf("data25: 0x%08X\n", data->data25);
    printf("data26: 0x%04X\n", data->data26);
}

void can_send_receive_reassemble_fragment_data(int socket_fd) 
{
    BR_DATA br_data;
    struct can_frame send_can_request;
    
    memset(&br_data, 0, sizeof(BR_DATA));

    send_can_request.can_id = CAN_REQUEST_ID;  // CAN ID for heartbeat
    send_can_request.can_dlc = 8;          // Data length
    // Set All bytes to 0
    memset(&send_can_request.data[0], 0, 8);
  
    printf("TCP to ETU: Data Read Request sent.\n");  
    /*
     * send_can_request
     */
    send_can_message(socket_fd, &send_can_request);
    /*
     * Data Read Response & Data ReadAcknowledgement
     */ 
    receive_can_response(socket_fd, &br_data);

    printf("ETU Response: Displaying all received data.\n");
    /*
     *Print the data structure
     */
    print_br_data(&br_data);

    printf("\n\n");

}


int main() {
    pthread_t thread_id;
    int socket_fd;
    struct sockaddr_can addr;
    struct ifreq ifr;


    // Setup CAN interface
    setup_can_interface(CAN_INTERFACE, CAN_BITRATE);

    // Create CAN socket
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

    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        print_error("Error binding socket to CAN interface");
    }

    // Create the heartbeat thread and pass socket_fd
    if (pthread_create(&thread_id, NULL, heartbeat_thread, &socket_fd) != 0) {
        print_error("Error creating heartbeat thread");
    }

    // Main function can perform other tasks
    while (1) {
        sleep(1); // Simulating other processing in main
       
	can_send_receive_reassemble_fragment_data(socket_fd);


    }

    close(socket_fd);
    return 0;
}

