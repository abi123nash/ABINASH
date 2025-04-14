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


#define CAN_INTERFACE   "can0"
//#define CAN_BITRATE     125000
#define CAN_BITRATE     1000000

#define CAN_REQUEST_ID  0x01200333
#define CAN_RESPONSE_ID 0x01210333
#define CAN_ACK_ID      0x01290333
#define Heartbeat_ID    0x017E0333

/*
 *Polynomial for CRC-16
 */
#define CRC_POLY        0xA001

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


void print_error(const char *msg) 
{
    printf("APP Error: %s\n",msg);
}


/**
 * @brief Sets up the CAN interface with the specified bitrate.
 *
 * This function configures a given CAN interface by bringing it down,
 * setting the specified bitrate, and then bringing it back up.
 * It uses system commands to execute these operations.
 *
 * @param interface The name of the CAN interface (e.g., "can0").
 * @param bitrate The bitrate to configure for the CAN interface (e.g., 500000).
 */
void setup_can_interface(const char *interface, int bitrate)
{
    char command[128];

    /*
     * Bring down the CAN interface before configuration
     */
    snprintf(command, sizeof(command), "ip link set %s down", interface);
    if (system(command) != 0)
    {
        fprintf(stderr, "Failed to bring down CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    /*
     * Configure the CAN interface with the specified bitrate
     */
    snprintf(command, sizeof(command), "ip link set %s up type can bitrate %d", interface, bitrate);
    if (system(command) != 0)
    {
        fprintf(stderr, "Failed to configure CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    printf("Successfully set up CAN interface %s with bitrate %d\n", interface, bitrate);

    /*
     * Bring up the CAN interface after configuration
     */
    snprintf(command, sizeof(command), "ip link set %s up", interface);
    if (system(command) != 0)
    {
        fprintf(stderr, "Failed to bring up CAN interface: %s\n", interface);
        exit(EXIT_FAILURE);
    }

    printf("Successfully brought up CAN interface: %s\n\n", interface);
}

/**  
 * @brief Send a CAN message  
 *  
 * This function sends a CAN frame using the provided socket file descriptor.  
 * It prints the frame details after sending and reports any errors if the transmission fails.  
 *  
 * @param socket_fd  File descriptor of the CAN socket  
 * @param frame      Pointer to the CAN frame to be sent  
 *  
 * @return 0 on success, -1 on failure  
 */  
int send_can_message(int socket_fd, struct can_frame *frame) 
{
    static uint8_t one_time = 0;
    fd_set write_fds;
    struct timeval timeout;
    int ret;
 
    /*
     *29-bit ID
     */ 
    frame->can_id = frame->can_id | CAN_EFF_FLAG; 

    /*  
     * Initialize file descriptor set and timeout  
     */
    FD_ZERO(&write_fds);
    FD_SET(socket_fd, &write_fds);
    timeout.tv_sec = 2;   
    timeout.tv_usec = 0;

    /*  
     * Use select to check if socket is ready for writing  
     */
    ret = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);
    if (ret <= 0) 
    {
        print_error("Timeout: Unable to send CAN message within 1 second");
        return -1;
    }

    /*  
     * Send the CAN frame  
     */
    if (write(socket_fd, frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) 
    {
        print_error("Error sending CAN frame");
        return -1;
    }

    /*  
     * Allow heartbeat messages to print only once  
     */
    if (frame->can_id == (Heartbeat_ID | CAN_EFF_FLAG)) 
    {
        if (one_time) 
        {
            return 0;
        }
        one_time = 1;
    }

    printf("CAN frame sent: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) 
    {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");

    return 0;
}

/**  
 * @brief Receive a CAN message  
 *  
 * This function receives a CAN frame from the provided socket file descriptor.  
 * It prints the received frame details and reports any errors if reading fails.  
 *  
 * @param socket_fd  File descriptor of the CAN socket  
 * @param frame      Pointer to store the received CAN frame  
 *  
 * @return 0 on success, -1 on failure  
 */  
int receive_can_message(int socket_fd, struct can_frame *frame) 
{
    fd_set read_fds;
    struct timeval timeout;
    int ret;
    ssize_t nbytes;

    /*  
     * Initialize file descriptor set and timeout  
     */
    FD_ZERO(&read_fds);
    FD_SET(socket_fd, &read_fds);
    timeout.tv_sec = 2;   // 1 second timeout
    timeout.tv_usec = 0;

    /*  
     * Use select to wait for data availability  
     */
    ret = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret <= 0) 
    {
        print_error("Timeout: No CAN message received within 1 second");
        return -1;
    }

    /*  
     * Read the CAN frame  
     */
    nbytes = read(socket_fd, frame, sizeof(struct can_frame));
    if (nbytes < 0) 
    {
        print_error("Error receiving CAN frame");
        return -1;
    }

    printf("Received CAN frame: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) 
    {
        printf("%02X ", frame->data[i]);
    }
    printf("\n");

    return 0;
}

/**
 * @brief Heartbeat thread function
 *
 * This function runs in a separate thread and periodically sends a heartbeat
 * message on the CAN bus every 500 milliseconds. The uptime counter is sent
 * in the first 4 bytes of the CAN frame, and the remaining bytes are set to zero.
 *
 * @param arg Pointer to the CAN socket file descriptor
 *
 * @return NULL (Thread function does not return a value)
 */
void *heartbeat_thread(void *arg)
{
    int socket_fd = *(int *)arg; /* Get socket file descriptor from main */
    uint32_t uptime = 0;
    struct can_frame heartbeat_msg;
    uint8_t ret=0;

    /*
     * Set the CAN ID and Data Length Code (DLC)
     *
     * TCP Heartbeat: 0x017E0333 | 0x00 0x00 0x00 0x01 0x00 0x00 0x00 0x00 | Classic CAN | 8 bytes | RTR Data
     *
     */
    heartbeat_msg.can_id = Heartbeat_ID; /* CAN ID for heartbeat */
    heartbeat_msg.can_dlc = 8;           /* Data length (8 bytes) */

    printf("Heartbeat: This CAN frame is sent every 500ms.\n");

    while (1)
    {
        /*
         * Set the uptime value in the first 4 bytes
         */
        heartbeat_msg.data[0] = (uint8_t)(uptime >> 24);
        heartbeat_msg.data[1] = (uint8_t)(uptime >> 16);
        heartbeat_msg.data[2] = (uint8_t)(uptime >> 8);
        heartbeat_msg.data[3] = (uint8_t)uptime;

        /*
         * Set the remaining bytes to 0
         */
        memset(&heartbeat_msg.data[4], 0, 4);

        /*
         * Send the heartbeat message over the CAN bus
         */
        ret = send_can_message(socket_fd, &heartbeat_msg);
        if (0 != ret)
        {
            print_error("heartbeat_msg CAN send failed!!");
        }


        /*
         * Increment uptime and sleep for 500 milliseconds
         */
        uptime++;
        usleep(500000);
    }

    return NULL;
}

/*  
 * Function to calculate CRC-16 over a given data buffer.  
 * Uses the standard CRC polynomial for error detection.  
 * Returns the computed CRC value.  
 */  
uint16_t calculate_crc(uint8_t *data, int length) 
{
    uint16_t crc = 0xFFFF;
    
    for (int i = 0; i < length; i++) 
    {
        crc ^= data[i];
        
        for (int j = 0; j < 8; j++) 
        {
            if (crc & 1) 
            {
                crc = (crc >> 1) ^ CRC_POLY;
            } 
            else 
            {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/*  
 * Function to receive a CAN response and reassemble fragmented data.  
 * Verifies CRC for each received frame and sends acknowledgment.  
 * Returns 0 on success, -1 on failure.  
 */  
int receive_can_response(int socket_fd, BR_DATA *br_data) 
{
    struct can_frame 		frame;
    struct can_frame 		ack_frame;
    uint8_t 			received_data[56] = {0};
    int 			received_bytes = 0;
    uint8_t 			frame_index;
    int 			expected_frames = 12;
    uint8_t 			ret = 0;
    uint16_t 			crc_received;

    for (int i = 0; i < expected_frames; i++) 
    {
        /*  
         * Receiving CAN frame from ETU to TCP.  
         * Each frame contains a part of the data. 
	 *
	 * ETU Response 1: 0x01210333 | 0x01 0x01 0x00 0x00 0x00 0x01 0xFF 0xFC | Classic CAN | 8 bytes | RTR Data
	 *
         */ 

        ret = receive_can_message(socket_fd, &frame); 
        if (0 != ret) 
        {
            print_error("CAN response receive failed");
            return -1;
        }
	
	printf("ETU to TCP: Data Read Response received.\n");

        frame_index = frame.data[0] - 1;
       
       	crc_received = (frame.data[6] << 8) | frame.data[7];

        /*  
         * Verify the CRC for the received frame.  
         * If CRC does not match, discard the frame.  
         */  
        if (calculate_crc(frame.data, 6) != crc_received) 
        {
            printf("CRC Error on frame %d\n", frame_index);
            return -1;
        }

        /*  
         * Copy valid data from the received frame.  
         * Data is reassembled based on frame index.  
         */
        if(frame_index == 11)
	{
         memcpy(&received_data[55], &frame.data[1], 1);
	}
       else
       {
         memcpy(&received_data[frame_index * 5], &frame.data[1], 5);
       }	       

        /*  
         * Prepare and send acknowledgment frame back to ETU.  
         * Acknowledgment contains frame index and confirmation bytes. 
	 *
	 * TCP Frame Ack 1:0x01290333 | 0x01 0x00 0x00 0x00 0x00 0x00 0xFF 0xFE | Classic CAN | 8 bytes | RTR Data
         */  
        ack_frame.can_id = CAN_ACK_ID + (frame_index * 5);
        ack_frame.can_dlc = 8;
        memset(ack_frame.data, 0, 8);
        ack_frame.data[0] = frame_index + 1;
        ack_frame.data[6] = 0xFF;
        ack_frame.data[7] = 0xFE - frame_index;
        
        printf("TCP to ETU: Data Read Acknowledgment sent.\n");
        ret = send_can_message(socket_fd, &ack_frame);
        if (0 != ret) 
        {
            print_error("CAN ACK send failed");
            return -1;
        }
	printf("\n");
    }
    
    /*  
     * Reception complete: All fragmented data has been successfully reassembled.  
     */  
    printf("Reception complete: All fragmented data has been successfully reassembled.\n");
    memcpy(br_data, received_data, sizeof(BR_DATA));
    return 0;
}

/*
 * Function to print the contents of BR_DATA structure
 */
void print_br_data(const BR_DATA *data)
{
    const uint16_t *ptr = (const uint16_t *)data;

    for (int i = 0; i < 23; i++)
    {
        printf("data%d: 0x%04X\n", i + 1, ptr[i]);
    }

    /*
     * Print 32-bit values separately
     */
    printf("data24: 0x%08X\n", data->data24);
    printf("data25: 0x%08X\n", data->data25);
    printf("data26: 0x%04X\n", data->data26);
}

/**  
 * @brief Sends a CAN request, receives fragmented data, and reassembles it.  
 *  
 * This function sends a data read request over CAN, receives the fragmented  
 * response, reassembles the data, and prints the structured information.  
 * It ensures the received data is validated and correctly processed.  
 *  
 * @param socket_fd: The CAN socket file descriptor.  
 *  
 * @return Returns 0 on success, -1 on failure.  
 */  
int can_send_receive_reassemble_fragment_data(int socket_fd)  
{  
    BR_DATA br_data;  
    struct can_frame send_can_request;  
    uint8_t ret = 0;  

    memset(&br_data, 0, sizeof(BR_DATA));  

    /*  
     * Configure the CAN request frame 
     *
     * TCP Request: 0x01200333 | --------------------------------------- | Classic CAN | 0 bytes | RTR Remote
     *
     */  
    send_can_request.can_id = CAN_REQUEST_ID; /* CAN ID for the request */  
    send_can_request.can_dlc = 8; /* Data length */  
    memset(&send_can_request.data[0], 0, 8); /* Initialize data to zero */  

    printf("TCP to ETU: Data Read Request sent.\n");  

    /*  
     * Send the request frame  
     */  
    ret = send_can_message(socket_fd, &send_can_request);  
    if (0 != ret)  
    {  
        print_error("CAN request send failed\n");  
        return -1;  
    }  

    /*  
     * Receive the fragmented data response and acknowledge each frame  
     */  
    ret = receive_can_response(socket_fd, &br_data); 
    if (0 != ret)  
    {  
        print_error("ETU Response failed!\n");  
        return -1;  
    }  

    printf("ETU Response: Displaying all received data.\n");  

    /*  
     * Print the reassembled data  
     */  
    print_br_data(&br_data);  

    printf("\n\n");  

    return 0;  
}  

/**
 * @brief Main function to initialize and manage CAN communication.
 *
 * This function sets up the CAN interface, creates a CAN socket,
 * starts a heartbeat thread, and continuously handles CAN data
 * transmission and reception in the main loop.
 *
 * @return Returns 0 on success, -1 on failure.
 */
int main()
{
    pthread_t thread_id;
    int socket_fd;
    struct sockaddr_can addr;
    struct ifreq ifr;
    uint8_t ret = 0;

    /*
     * Setup the CAN interface
     */
    setup_can_interface(CAN_INTERFACE, CAN_BITRATE);

    /*
     * Create a CAN socket
     */
    socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd < 0)
    {
        print_error("Socket creation failed");
        return -1;
    }

    /*
     * Retrieve the CAN interface index
     */
    strcpy(ifr.ifr_name, CAN_INTERFACE);
    if (ioctl(socket_fd, SIOCGIFINDEX, &ifr) < 0)
    {
        print_error("Error getting CAN interface index");
        return -1;
    }

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    /*
     * Bind the socket to the CAN interface
     */
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        print_error("Error binding socket to CAN interface");
        return -1;
    }

    /*
     * Create the heartbeat thread and pass socket_fd
     */
    if (pthread_create(&thread_id, NULL, heartbeat_thread, &socket_fd) != 0)
    {
        print_error("Error creating heartbeat thread");
        return -1;
    }

    /*
     * Main loop: Continuously handle CAN communication
     */
    while (1)
    {
        sleep(1); /* Simulating other processing in main */

        ret = can_send_receive_reassemble_fragment_data(socket_fd);
        if (0 != ret)
        {
            print_error("ETU Response failed! Try again:\n");
        }
    }

    /*
     * Close the CAN socket before exiting
     */
    close(socket_fd);

    return 0;
}

