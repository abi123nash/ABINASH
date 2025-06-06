/**
 *  @file    modbus_can_bridge.c
 *  @brief   Modbus-TCP to CAN Protocol Translation & Communication Handler
 *
 *  This application acts as a middleware between a PC-based Modbus TCP client
 *  and remote CAN modules. It listens for Modbus read requests, extracts CAN
 *  identifiers, constructs appropriate CAN read requests, transmits them over 
 *  the CAN interface, receives the corresponding CAN response, maps the data 
 *  to Modbus registers, and sends the result back to the Modbus client.
 *
 *  Architecture Overview:
 *  [PC Modbus Client] ---> [Modbus-TCP Server on this App] ---> [CAN Request]
 *                                                              |
 *                                       <--- [CAN Response] <---
 *
 *  @version 1.0.0
 *  @date    2025-04-07
 *  @author  Abinash
 *  @email   projects@kemsys.com
 *
 *  @copyright
 *  Copyright (c) 2025 Kemsys Technologies. All rights reserved.
 *
 *  @note
 *  - Requires `libmodbus` for TCP server functionality.
 *  - Assumes CAN interface is properly initialized (e.g., `can0`).
 *
 *  @bug No known bugs.
 */

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
#include <modbus/modbus.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <fcntl.h>
#include "register_details.h"
#include "log.h"


#define BROADCAST_PORT 12345
#define BUF_SIZE 1024
#define NEED_IP_MSG "NEED_IP"



#define CAN_INTERFACE       "can0"
#define CAN_BITRATE         1000000
#define Heartbeat_ID        0x017E0333

#define MAX_RETRIES         3


/* CAN Message IDs */
#define CAN_READ_REQ_MSG_ID            0  /* CAN: Read Request from Host */
#define CAN_RESPONSE_MSG_ID            1  /* CAN: Response to Read Request */
#define CAN_READ_ACK_MSG_ID            9  /* CAN: Acknowledgement for Read */

/* TCP ↔ ETU Write Operation Message IDs */
#define TCP_TO_ETU_WRITE_REQ_ID        2  /* TCP to ETU: Write Request */
#define ETU_TO_TCP_WRITE_GRANT_ID      3  /* ETU to TCP: Grant Write Request */
#define TCP_TO_ETU_WRITE_CMD_ID        4  /* TCP to ETU: Write Command with Data */
#define ETU_TO_TCP_WRITE_ACK_ID        5  /* ETU to TCP: Acknowledgement for Write */
#define TCP_TO_ETU_WRITE_TERM_ID       6  /* TCP to ETU: Write Termination */
#define ETU_TO_TCP_WRITE_TERM_ID       7  /* ETU to TCP: Write Termination Acknowledgement */


#define CAN_DATA_LEN 8


#define CAN_READ_TIME          2
#define CAN_MAX_BYTE_SIZE      6

#define BYTE1    8

#define CLEAR_NONE             0x00
#define CLEAR_BITS             0x01
#define CLEAR_INPUT_BITS       0x02
#define CLEAR_REGISTERS        0x04
#define CLEAR_INPUT_REGISTERS  0x08



/***************************************************************
 *  Debug Utilities
 ***************************************************************/
void debug_print(const char *func, int line) {
    LOG_DEBUG("Kemsys Debug -> Function: %s | Line: %d\n", func, line);
}

/*
 * Macro to simplify usage of the debug print function
 */
#define p() debug_print(__func__, __LINE__)



/*
 * EE_prom path 
 */

#define EEPROM_PATH "/sys/bus/spi/drivers/at25/spi1.0/eeprom"




/***************************************************************
 *  Modbus & CAN Configuration
 ***************************************************************/

#define SERVER_PORT         502
#define MAX_ADU_LENGTH      50


/*  
 * Modbus Function Codes (0x00 to 0x0F) with Descriptions and Mappings  
 */  

#define MODBUS_FUNC_RESERVED_0                   0   /* Reserved – Not used */
#define MODBUS_FUNC_READ_COILS                   1   /* Read Coils -> mb_mapping->tab_bits */
#define MODBUS_FUNC_READ_DISCRETE_INPUTS         2   /* Read Discrete Inputs -> mb_mapping->tab_input_bits */
#define MODBUS_FUNC_READ_HOLDING_REGISTERS       3   /* Read Holding Registers -> mb_mapping->tab_registers */
#define MODBUS_FUNC_READ_INPUT_REGISTERS         4   /* Read Input Registers -> mb_mapping->tab_input_registers */
#define MODBUS_FUNC_WRITE_SINGLE_COIL            5   /* Write Single Coil -> mb_mapping->tab_bits */
#define MODBUS_FUNC_WRITE_SINGLE_REGISTER        6   /* Write Single Register -> mb_mapping->tab_registers */
#define MODBUS_FUNC_READ_EXCEPTION_STATUS        7   /* Read Exception Status – Not commonly used */
#define MODBUS_FUNC_DIAGNOSTICS                  8   /* Diagnostics – Implementation specific */
#define MODBUS_FUNC_GET_COM_EVENT_COUNTER        11  /* Get Comm Event Counter – Optional */
#define MODBUS_FUNC_GET_COM_EVENT_LOG            12  /* Get Comm Event Log – Optional */
#define MODBUS_FUNC_WRITE_MULTIPLE_COILS         15  /* Write Multiple Coils -> mb_mapping->tab_bits */
#define MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS     16  /* Write Multiple Registers -> mb_mapping->tab_registers */

/* Notes:  
 * Function codes 9, 10, 13, and 14 are skipped as they are reserved or rarely used.
 * For implementation, verify optional codes based on device support.
 */


#define MODBUS_ALLOC_NUM_COILS              9000   // For tab_bits
#define MODBUS_ALLOC_NUM_DISCRETE_INPUTS    9000   // For tab_input_bits
#define MODBUS_ALLOC_NUM_HOLDING_REGISTERS  9000   // For tab_registers
#define MODBUS_ALLOC_NUM_INPUT_REGISTERS    9000   // For tab_input_registers




/**************************************************************
 * Dataset Category to Index Mapping
 *
 * Category                              | Index
 * --------------------------------------|-------
 * Commands                              | 0
 * Event Data Transmit                   | 1
 * Settings                              | 2
 * Metering                              | 3
 * Breaker Status                        | 4
 * Records (Trip, Events, Maintenance)   | 5
 * HeartBeat                             | 6
 **************************************************************/

/**************************************************************
 * Dataset to Category Mapping
 *
 * Dataset Name           | Category
 * ------------------------|------------------------
 * Status                 | Breaker Status
 * Monitoring Data        | Metering
 * Breaker Data           | Breaker Status
 * Protection Settings    | Settings
 * General Settings       | Settings
 * Module Settings        | Settings
 * Commands               | Commands
 * Data Records           | Records
 * Product Info RS-485    | Breaker Status
 * Module Data            | Breaker Status
 * User defined Map       | Breaker Status
 **************************************************************/


#define CMD_COMMANDS                0
#define CMD_BREAKER_STATUS          2 
#define CMD_METERING                3
#define CMD_SETTINGS                1
#define CMD_Trip_ECORDS             4
#define CMD_Events_RECORDS          5
#define CMD_Maintainence_RECORD     6
#define CMD_HEARTBEAT               7


device_data *tcp_data[] = {
    module_data_TCP,
    module_settings_TCP,
};

const int tcp_dataset_counts[] = {
    sizeof(module_data_TCP) / sizeof(module_data_TCP[0]),
    sizeof(module_settings_TCP) / sizeof(module_settings_TCP[0])
};

device_data *all_datasets[] = {
    status,
    monitoring_data,
    breaker_data,
    protection_settings,
    general_settings,
    module_settings,
    module_data,
    commands,
    data_records,
    Product_Info_RS_485,
    Product_Info_PC_HMI,
    User_Defined_Map
};

const int dataset_counts[] = {
    sizeof(status) / sizeof(status[0]),
    sizeof(monitoring_data) / sizeof(monitoring_data[0]),
    sizeof(breaker_data) / sizeof(breaker_data[0]),
    sizeof(protection_settings) / sizeof(protection_settings[0]),
    sizeof(general_settings) / sizeof(general_settings[0]),
    sizeof(module_settings) / sizeof(module_settings[0]),
    sizeof(module_data) / sizeof(module_data[0]),
    sizeof(commands) / sizeof(commands[0]),
    sizeof(data_records) / sizeof(data_records[0]),
    sizeof(Product_Info_RS_485) / sizeof(Product_Info_RS_485[0]),
    sizeof(Product_Info_PC_HMI) / sizeof(Product_Info_PC_HMI[0]),
    sizeof(User_Defined_Map) / sizeof(User_Defined_Map[0])
};


typedef struct {
    const char *dataset_name;  // Dataset name like "monitoring_data"
    int data_header;           // Mapped command category
} dataheater_mapping;

static dataheater_mapping header[] = {
    { "status",              CMD_BREAKER_STATUS },
    { "monitoring_data",     CMD_METERING },
    { "breaker_data",        CMD_BREAKER_STATUS },
    { "protection_settings", CMD_SETTINGS },
    { "general_settings",    CMD_SETTINGS },
    { "module_settings",     CMD_SETTINGS },
    { "module_data",         CMD_BREAKER_STATUS },
    { "commands",            CMD_COMMANDS },
    { "data_records",        CMD_Trip_ECORDS },
    { "Product_Info_RS_485", CMD_BREAKER_STATUS },
    { "Product_Info_PC_HMI", CMD_BREAKER_STATUS },
    { "User_Defined_Map",    CMD_BREAKER_STATUS }
};


/***************************************************************
 *  Application Buffers & Constants
 ***************************************************************/
uint8_t received_data[500] = {0};

/*  
 * Check CAN interface up or not  
 */
int is_interface_up(const char *ifname)
{
    struct ifreq ifr;
    int is_up;
    int is_running;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0)
    {
        LOG_ERROR("socket failed while checking interface state\n");
        return 0;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';  // Ensure null-termination

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
    {
        LOG_ERROR("ioctl failed while checking interface state\n");
        close(sock);
        return 0;
    }

    close(sock);

    is_up = (ifr.ifr_flags & IFF_UP);
    is_running = (ifr.ifr_flags & IFF_RUNNING);

    if (!is_up || !is_running)
    {
        LOG_DEBUG("Interface %s flags: 0x%x (IFF_UP=%d, IFF_RUNNING=%d)\n",
                  ifname, ifr.ifr_flags, is_up, is_running);
    }

    return is_up && is_running;
}


/*  
 * Check CAN state is ERROR-ACTIVE  
 */
int is_can_state_ok(const char *ifname)
{
    FILE *fp;
    char command[256], state[64];
    int found = 0;

    snprintf(command, sizeof(command), "ip -details link show %s", ifname);

    fp = popen(command, "r");
    if (!fp)
    {
        LOG_ERROR("Failed to run ip command\n");
        return 0;
    }

    while (fgets(state, sizeof(state), fp) != NULL)
    {
        if (strstr(state, "state") && strstr(state, "ERROR-ACTIVE"))
        {
            found = 1;
            break;
        }
    }

    pclose(fp);

    if (!found)
    {
        LOG_WARN("CAN interface %s is NOT in ERROR-ACTIVE state\n", ifname);
    }

    return found;
}


/**
 * @brief Sets up the CAN interface with the specified bitrate.
 *
 * This function configures a given CAN interface by bringing it down,
 * setting the specified bitrate, and then bringing it back up.
 * It uses system commands to execute these operations.
 *
 * @param interface The name of the CAN interface (e.g., "can0").
 * @param bitrate   The bitrate to configure for the CAN interface (e.g., 500000).
 */

void setup_can_interface(const char *interface, int bitrate)
{
    char command[256];
    int attempt = 0;
    int ret;

    while (attempt < MAX_RETRIES)
    {
        LOG_DEBUG("Attempt %d: Setting up CAN interface: %s\n", attempt + 1, interface);

        /*
         * Step 1: Bring down interface
         */
        snprintf(command, sizeof(command), "ip link set %s down", interface);
        if (system(command) != 0)
        {
            LOG_ERROR("Failed to bring down CAN interface: %s\n", interface);
            exit(EXIT_FAILURE);
        }

        /*
         * Step 2: Set bitrate
         */
        snprintf(command, sizeof(command), "ip link set %s type can bitrate %d", interface, bitrate);
        if (system(command) != 0)
        {
            LOG_ERROR("Failed to configure CAN bitrate on interface: %s\n", interface);
            exit(EXIT_FAILURE);
        }

        /*
         * Step 3: Bring interface up
         */
        snprintf(command, sizeof(command), "ip link set %s up", interface);
        ret = system(command);  
        ret = system(command);  
        if (ret != 0)
        {
            LOG_ERROR("Failed to bring up CAN interface: %s\n", interface);
            attempt++;
            continue;
        }

        /*
         * Step 4: Check interface UP and RUNNING
         */
        if (!is_interface_up(interface))
        {
            LOG_ERROR("CAN interface %s is not UP and RUNNING\n", interface);
            attempt++;
            continue;
        }

        /*
         * Step 5: Check CAN state
         */
        if (!is_can_state_ok(interface))
        {
            LOG_ERROR("CAN interface %s is not in ERROR-ACTIVE state\n", interface);
            attempt++;
            continue;
        }

        LOG_DEBUG("Successfully brought up CAN interface: %s with bitrate %d\n\n", interface, bitrate);
        return; // success
    }

    LOG_ERROR("CAN interface %s failed to initialize after %d attempts. Exiting.\n", interface, MAX_RETRIES);
    exit(EXIT_FAILURE);
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
    static uint8_t        one_time = 0;
    fd_set                write_fds;
    struct timeval        timeout;
    int                   ret;
    uint8_t               i;

    /*
     * 29-bit ID
     */
    frame->can_id = frame->can_id | CAN_EFF_FLAG;

    /*
     * Initialize file descriptor set and timeout
     */
    FD_ZERO(&write_fds);
    FD_SET(socket_fd, &write_fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    /*
     * Use select to check if socket is ready for writing
     */
    ret = select(socket_fd + 1, NULL, &write_fds, NULL, &timeout);
    if (ret <= 0)
    {
        LOG_ERROR("Timeout: Unable to send CAN message within 2 seconds\n");
        return -1;
    }

    /*
     * Send the CAN frame
     */
    if (write(socket_fd, frame, sizeof(struct can_frame)) != sizeof(struct can_frame))
    {
        LOG_ERROR("Error sending CAN frame\n");
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

    LOG_DEBUG("CAN frame sent: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (i = 0; i < frame->can_dlc; i++)
    {
        log("%02X ", frame->data[i]);
    }
    log("\n");

    return 0;
}


/**
 * @brief Receive a specific CAN message within 2 seconds
 *
 * This function waits up to 2 seconds to receive a CAN frame matching a specific CAN ID.
 * It filters incoming messages until the expected ID is received or times out.
 *
 * @param socket_fd   File descriptor of the CAN socket
 * @param frame       Pointer to store the received CAN frame
 * @param can_req_id  Expected CAN ID to match
 *
 * @return 0 on success, -1 on failure (timeout or error)
 */
int receive_can_message_with_filter(int socket_fd, struct can_frame *frame, int can_resp_id)
{
    fd_set              read_fds;
    struct timeval      timeout;
    time_t              start_time;
    ssize_t             nbytes;
    int                 ret;
    uint8_t             i;


#if 0

    struct can_filter   rfilter;
    /*
     * ---------------------------------------------------------------
     * Configure CAN socket filter to accept only one specific ID
     * ---------------------------------------------------------------
     * - Accept only Extended (29-bit) CAN frames.
     * - Match exactly one extended CAN ID: `can_resp_id`.
     * - Reject all other CAN frames (both classic and other extended).
     * - Useful when communicating with a specific node on a shared bus.
     */  
    
    //setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

    rfilter.can_id   = (can_resp_id | CAN_EFF_FLAG);             // Match only extended ID flag
    rfilter.can_mask = (CAN_EFF_MASK | CAN_EFF_FLAG);           // Filter only by EFF flag

    if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0)
    {
       LOG_ERROR("Error setting CAN filter for Extended ID frames\n");
       return -1;
    }

#endif     


    /*
     * Record the start time for timeout tracking
     */
    start_time = time(NULL);

    /*
     * Loop for up to 2 seconds, polling for the expected CAN ID
     */
    while ((time(NULL) - start_time) < CAN_READ_TIME)
    {
        FD_ZERO(&read_fds);
        FD_SET(socket_fd, &read_fds);


        /*
         * Poll every 200 milliseconds
         */
        timeout.tv_sec = CAN_READ_TIME;
        timeout.tv_usec = 0;

        ret = select(socket_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret < 0)
        {
            LOG_ERROR("select() failed");
            return -1;
        }
        else if (ret == 0)
        {
            /*
             * No data yet, continue polling
             */
            continue;
        }

        /*
         * Read the CAN frame
         */
        nbytes = read(socket_fd, frame, sizeof(struct can_frame));
        if (nbytes < 0)
        {
            LOG_ERROR("read() failed");
            return -1;
        }

        /*
         * Check for expected CAN ID match
         */
        if (frame->can_id == (can_resp_id | CAN_EFF_FLAG))
        {
            LOG_DEBUG("Received expected CAN frame: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
            for (i = 0; i < frame->can_dlc; i++)
            {
                log("%02X ", frame->data[i]);
            }
            log("\n");
            return 0;
        }
    }

    /*
     * Timeout occurred before receiving the expected CAN ID
     */
    LOG_ERROR("Timeout: CAN frame with ID 0x%X not received within 2 seconds\n", can_resp_id | CAN_EFF_FLAG);
    return -1;
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
        LOG_ERROR("Timeout: No CAN message received within 1 second\n");
        return -1;
    }

    /*  
     * Read the CAN frame  
     */
    nbytes = read(socket_fd, frame, sizeof(struct can_frame));
    if (nbytes < 0) 
    {
        LOG_ERROR("Error receiving CAN frame\n");
        return -1;
    }

    LOG_DEBUG("Received CAN frame: ID=0x%X DLC=%d Data=", frame->can_id, frame->can_dlc);
    for (int i = 0; i < frame->can_dlc; i++) 
    {
        LOG_DEBUG("%02X ", frame->data[i]);
    }
    LOG_DEBUG("\n");

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

    LOG_DEBUG("Heartbeat: This CAN frame is sent every 500ms.\n");

    while (1)
    {continue;
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
            LOG_ERROR("heartbeat_msg CAN send failed!!\n");
        }


        /*
         * Increment uptime and sleep for 500 milliseconds
         */
        uptime++;
        usleep(500000);
    }

    return NULL;
}

/**************************************************************************************************/
/* Function Name : GenerateCRC			   	      			                  */
/* Description   : Calculates general-purpose CRC for given data.                                 */
/*                                                                                                */
/* Parameters    : ui8_data: The pointer to data for CRC calculati                                */
/*                 ui16_size : Data Length for calculation.                                       */
/*                 ui16_crc_start : Start data index for CRC calculation.                         */
/*                                                                                                */
/* Return        : Calculated CRC. 			                                          */
/*                                                                                                */
/**************************************************************************************************/
uint16_t GenerateCRC(uint8_t * ui8_data, uint16_t ui16_size)
{
	uint16_t  ui16_loopCount = 0U;
	uint16_t  ui16_calc_crc = 0;

	for(ui16_loopCount = 0; ui16_loopCount < ui16_size; ui16_loopCount++)
	{
		ui16_calc_crc += ui8_data[ui16_loopCount];		/* Set calculation data for CRC */
	}

	ui16_calc_crc = ~ui16_calc_crc;

	return ui16_calc_crc;

}


/*  
 * Function to receive a CAN response and reassemble fragmented data.  
 * Verifies CRC for each received frame and sends acknowledgment.  
 * Returns 0 on success, -1 on failure.  
 */  

int receive_can_response(int socket_fd, int canid, int size) 
{
    struct can_frame       frame;                    /* Frame to receive CAN data */
    struct can_frame       ack_frame;                /* Frame to send CAN acknowledgment */
    uint8_t                ret = 0;                  /* Return value for send/receive functions */
    uint16_t               crc_received = 0;         /* CRC value extracted from CAN frame */
    int                    remaining_size = size;    /* Track remaining bytes to be received */
    int                    frame_index = 0;          /* Frame sequence index */
    int                    bytes_to_copy = 0;        /* Number of data bytes to copy */
    int                    byte_index = 0;           /* Index used in copy loop */
    static int             received_data_index = 0;  /* Buffer index for reassembled data */
    int                    can_res_id = canid;       /* CAN response ID */
    int                    can_ack_id = canid;       /* CAN acknowledgment ID */

    /*  
     * Set bits 16-19 of CAN ID to indicate "Read Response"
     * Make the response ID
     */
    can_res_id &= ~(0xF << 16);   
    can_res_id |=  (CAN_RESPONSE_MSG_ID << 16);      

    /*  
     * Set bits 16-19 of CAN ID to indicate "Read ACK"
     */
    can_ack_id &= ~(0xF << 16);    
    can_ack_id |=  (CAN_READ_ACK_MSG_ID << 16);       

    received_data_index = 0;	

    while (remaining_size > 0) 
    {
        /*  
         * Receiving CAN frame from ETU to TCP.  
         * Each frame contains a part of the data.  
         */

        ret = receive_can_message_with_filter(socket_fd, &frame, can_res_id);
        if (ret != 0) 
        {
            LOG_ERROR("CAN response receive failed\n");
            return -1;
        }

        LOG_DEBUG("ETU to TCP: Data Read Response received.\n");

        crc_received  = (frame.data[6] << 8) | frame.data[7];

        /*  
         * Verify CRC for received frame  
         */
        if (GenerateCRC(frame.data, 6) != crc_received) 
        {
            LOG_ERROR("CRC Error on frame %d\n", frame_index);
            return -1;
        }

        /*  
         * Determine how many bytes to copy from this frame  
         */
        bytes_to_copy = (remaining_size >= CAN_MAX_BYTE_SIZE) ? CAN_MAX_BYTE_SIZE : remaining_size;

        for (byte_index = 0; byte_index < bytes_to_copy; byte_index++) 
        {
            received_data[received_data_index++] = frame.data[byte_index];
        }

        remaining_size -= bytes_to_copy;

        /*  
         * Prepare and send acknowledgment frame  
         */
        ack_frame.can_id    = can_ack_id;            /* Data Read Acknowledgment ID */
        ack_frame.can_dlc   = CAN_DATA_LEN;
        memset(ack_frame.data, 0, CAN_DATA_LEN);
        ack_frame.data[6]   = 0xff;
        ack_frame.data[7]   = 0xff;


        ret = send_can_message(socket_fd, &ack_frame);
        if (ret != 0) 
        {
            LOG_ERROR("CAN ACK send failed\n");
            return -1;
        }

        LOG_DEBUG("TCP to ETU: Data Read Acknowledgment sent.\n");
        can_res_id += 3;
        can_ack_id += 3;
        LOG_DEBUG("\n\n");
    }

    /*  
     * Reception complete, store received data  
     */
    LOG_DEBUG("Reception complete: All fragmented data has been successfully reassembled.\n");

    return 0;
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
int can_txrx_reassemble_frag_data_read(int socket_fd,int canid,int size,uint8_t fun_code)  
{  
    struct can_frame send_can_request;  
    uint8_t ret = 0;  
    unsigned short crc=0;

    LOG_DEBUG("CAN Read communication will start: Preparing to send read request to CAN ID = %d (0x%X)\n\n", canid, canid);

    /*  
     * Configure the CAN request frame 
     *
     * TCP Request: 0x01200333 | --------------------------------------- | Classic CAN | 0 bytes | RTR Remote
     *
     */  
    send_can_request.can_id = canid;                        /* CAN ID for the request */  
    send_can_request.can_dlc = CAN_DATA_LEN;                /* Data length */  
    memset(&send_can_request.data[0], 0, CAN_DATA_LEN);     /* Initialize data to zero */ 

    send_can_request.data[0]=size; 

    /*
     * Construct the CRC
     */
    crc = GenerateCRC(send_can_request.data, 6);

    send_can_request.data[6] = (crc >> 8) & 0xFF;
    send_can_request.data[7] = crc & 0xFF;

    /*  
     * Send the request frame  
     */  
    ret = send_can_message(socket_fd, &send_can_request);  
    if (0 != ret)  
    {  
        LOG_ERROR("CAN request send failed\n");  
        return -1;  
    }  
    
    LOG_DEBUG("TCP to ETU: Data Read Request sent.\n\n");  

   /*
    * For Function Code 1 (Read Coils) and Function Code 2 (Read Discrete Inputs),
    * each address corresponds to 1 bit of data. Since Modbus sends data in full bytes,
    * we must round up the size to the next full byte if there are leftover bits.
    */
   if ((fun_code == 1) || (fun_code == 2))
   {
      if (((size / BYTE1) * BYTE1) < size)
      {
         /*  
          * If size is not a multiple of BYTE1 (i.e., 8 bits),
          * add one more byte to cover remaining bits  
          */
          size = (size / BYTE1) + 1;
       }
      else
       {
         /*  
          * Size fits exactly in whole bytes  
          */
         size = size / BYTE1;
      }
    }
   else 
    {
      /*
       * For all other function codes, each register is 2 bytes (1 word).
       * So multiply the number of registers (size) by 2 to get total byte size.
       */
       size = size * 2;
     }  

     
    /*  
     * Receive the fragmented data response and acknowledge each frame  
     */ 
  
    ret = receive_can_response(socket_fd , canid , size); 
    if (0 != ret)  
    {  
        LOG_ERROR("ETU Response failed!\n");  
        return -1;  
    }  

    LOG_DEBUG("ETU Response: Displaying all received data.\n");  

    LOG_DEBUG("\n\n");  

    return 0;  
}  

/**
 * @brief can_tx_rx_reassemble_frag_data_write
 *
 * This function sends fragmented write requests over the CAN bus and waits for
 * corresponding grant and acknowledgment frames. It ensures message integrity
 * by checking CRC for every sent and received frame. A final termination frame
 * is also sent to complete the transaction cleanly.
 *
 * @param socket_fd   Socket file descriptor for CAN communication
 * @param can_req_id  CAN ID to initiate write request
 * @param data        Pointer to data buffer to write
 * @param length      Length of data in words (each word = 2 bytes)
 *
 * @return 0 on success, -1 on failure
 */

int can_txrx_reassemble_frag_data_write(int socket_fd, uint32_t can_req_id, uint8_t *data, uint16_t length)
{
    struct can_frame       frame;
    int                    ret             = 0;
    unsigned short         crc             = 0;
    uint32_t               can_res_id      = can_req_id;
    uint32_t               can_tx_id       = can_req_id;
    uint32_t               can_tx_ack_id   = can_req_id;
    uint16_t               remaining_size  = length * 2;
    uint16_t               bytes_to_copy   = 0;
    uint16_t               send_data_index = 0;
    uint16_t               byte_index      = 0;
    uint8_t                frame_count     = 0;

    LOG_DEBUG("CAN Write communication will start: Preparing to send write request to CAN ID = %d (0x%X)\n\n", can_req_id, can_req_id);

    /*  
     * Configure the CAN Write Request frame
     *
     * TCP to ETU Write Request: 0x01200233 | ------------------------- | Classic CAN | 8 bytes | Data Frame
     * Payload Format: [Length][0][0][0][0][0][CRC_H][CRC_L]
     * MsgType = 0x2 (TCP_TO_ETU_WRITE_REQ_ID)
     */

    frame.can_id  = can_req_id;              /* CAN ID for TCP→ETU Write Request */
    frame.can_dlc = CAN_DATA_LEN;            /* Full CAN frame (8 bytes) */
    memset(&frame.data[0], 0, CAN_DATA_LEN); /* Initialize data buffer */

    frame.data[0] = length;                  /* First byte = data length */

    crc = GenerateCRC(frame.data, 6);        /* CRC over first 6 bytes */
    frame.data[6] = (crc >> 8) & 0xFF;       /* CRC High byte */
    frame.data[7] = crc & 0xFF;              /* CRC Low byte */

    ret = send_can_message(socket_fd, &frame); /* Send Write Request */
    if (ret != 0)
    {
        LOG_ERROR("TCP to ETU: Write Request frame send failed\n");
        return -1;
    }

    LOG_DEBUG("TCP to ETU: Write Request frame sent\n");

    /*
     * ------------------- ETU to TCP Write Grant -------------------
     * Expecting response with MsgType = 3 (ETU_TO_TCP_WRITE_GRANT_ID)
     */
    can_res_id &= ~(0xF << 16);                          /* Clear bits 16–19 (MsgType field) */
    can_res_id |= (ETU_TO_TCP_WRITE_GRANT_ID << 16);     /* Set MsgType = 3 for grant */

    ret = receive_can_message_with_filter(socket_fd, &frame, can_res_id);
    if (ret != 0)
    {
        LOG_ERROR("ETU to TCP: Write Grant frame receive failed\n");
        return -1;
    }

    LOG_DEBUG("ETU to TCP: Write Grant frame received\n\n");

    crc = (frame.data[6] << 8) | frame.data[7]; /* Extract CRC */
    if (GenerateCRC(frame.data, 6) != crc)
    {
        LOG_ERROR("CRC mismatch in Write Grant frame\n");
        return -1;
    }

    /*
     * ------------------- TCP to ETU Write Data Frames -------------------
     */
    can_tx_id     &= ~(0xF << 16);
    can_tx_id     |= (TCP_TO_ETU_WRITE_CMD_ID << 16);

    can_tx_ack_id &= ~(0xF << 16);
    can_tx_ack_id |= (ETU_TO_TCP_WRITE_ACK_ID << 16);

    while (remaining_size > 0)
    {
        frame.can_id  = can_tx_id;
        frame.can_dlc = CAN_DATA_LEN;
        memset(&frame.data[0], 0, CAN_DATA_LEN);

        bytes_to_copy = (remaining_size >= CAN_MAX_BYTE_SIZE) ? CAN_MAX_BYTE_SIZE : remaining_size;

        for (byte_index = 0; byte_index < bytes_to_copy; byte_index++)
        {
            frame.data[byte_index] = data[send_data_index++];
        }

        remaining_size -= bytes_to_copy;

        crc = GenerateCRC(frame.data, 6);
        frame.data[6] = (crc >> 8) & 0xFF;
        frame.data[7] = crc & 0xFF;

        ret = send_can_message(socket_fd, &frame);
        if (ret != 0)
        {
            LOG_ERROR("TCP to ETU: Data frame %d send failed\n", frame_count + 1);
            return -1;
        }

        LOG_DEBUG("TCP to ETU: Data frame %d sent\n", ++frame_count);

        ret = receive_can_message_with_filter(socket_fd, &frame, can_tx_ack_id);
        if (ret != 0)
        {
            LOG_ERROR("ETU to TCP: Data frame %d ACK receive failed\n", frame_count);
            return -1;
        }

        LOG_DEBUG("ETU to TCP: Data frame %d ACK received\n", frame_count);

        crc = (frame.data[6] << 8) | frame.data[7];
        if (GenerateCRC(frame.data, 6) != crc)
        {
            LOG_ERROR("CRC mismatch in ACK for frame %d\n", frame_count);
            return -1;
        }

        can_tx_id     += 3;
        can_tx_ack_id += 3;
    }

    /*
     * ------------------- TCP to ETU Write Termination -------------------
     */
    can_tx_id     &= ~(0xF << 16);
    can_tx_id     |= (TCP_TO_ETU_WRITE_TERM_ID << 16);

    can_tx_ack_id &= ~(0xF << 16);
    can_tx_ack_id |= (ETU_TO_TCP_WRITE_TERM_ID << 16);

    frame.can_id  = can_tx_id;
    frame.can_dlc = CAN_DATA_LEN;
    memset(&frame.data[0], 0, CAN_DATA_LEN);
    frame.data[6] = 0xFF;
    frame.data[7] = 0xFF;

    ret = send_can_message(socket_fd, &frame);
    if (ret != 0)
    {
        LOG_ERROR("TCP to ETU: Termination frame send failed\n");
        return -1;
    }

    LOG_DEBUG("TCP to ETU: Termination frame sent\n");

    ret = receive_can_message_with_filter(socket_fd, &frame, can_tx_ack_id);
    if (ret != 0)
    {
        LOG_ERROR("ETU to TCP: Termination ACK receive failed\n");
        return -1;
    }

    LOG_DEBUG("ETU to TCP: Termination ACK received\n");

    crc = (frame.data[6] << 8) | frame.data[7];
    if (GenerateCRC(frame.data, 6) != crc)
    {
        LOG_ERROR("CRC mismatch in Termination ACK frame\n");
        return -1;
    }

    LOG_DEBUG("Write operation successful without errors\n");

    return 0;
}


/**  
 * @brief get_own_ip  
 *  
 * This function retrieves the IP address of the current device (excluding loopback).  
 * It iterates over all network interfaces and returns the first valid IPv4 address.  
 * If no valid address is found, it returns the string "UNKNOWN".  
 *  
 * @return A dynamically allocated string containing the device IP address.  
 */  
char* get_own_ip()  
{
    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;
    char *ip = NULL;
    char host[NI_MAXHOST];

    /*  
     * Get the list of network interfaces  
     */  
    if (getifaddrs(&ifaddr) == -1)  
    {
        perror("getifaddrs");
        return strdup("UNKNOWN");
    }

    /*  
     * Loop through the interfaces and find the first non-loopback IPv4  
     */  
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)  
    {
        if ((ifa->ifa_addr != NULL) && (ifa->ifa_addr->sa_family == AF_INET) &&
            !(ifa->ifa_flags & IFF_LOOPBACK))  
        {
            if (getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                            host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)  
            {
                ip = strdup(host);
                break;
            }
        }
    }

    /*  
     * Free the interface list and return the IP or "UNKNOWN"  
     */  
    freeifaddrs(ifaddr);
    return ip ? ip : strdup("UNKNOWN");
}

/**  
 * @brief ip_response_thread  
 *  
 * This function runs as a background thread, listening for UDP broadcast messages.  
 * When it receives a specific request message ("NEED_IP"), it responds with its own IP address.  
 * The IP is determined using the get_own_ip() function and sent to the requesting client.  
 *  
 * @param arg : Not used  
 * @return NULL (thread exit)  
 */  
void* ip_response_thread(void* arg)  
{
    int sockfd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    char buffer[BUF_SIZE];
    socklen_t addr_len;
    ssize_t recv_len;
    char *own_ip;

    addr_len = sizeof(client_addr);

    /*  
     * Create UDP socket  
     */  
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)  
    {
        LOG_ERROR("Socket creation failed\n");
        pthread_exit(NULL);
    }

    /*  
     * Bind to broadcast port and any local IP  
     */  
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BROADCAST_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)  
    {
        LOG_ERROR("Bind failed\n");
        close(sockfd);
        pthread_exit(NULL);
    }

    LOG_DEBUG("Listening for NEED_IP messages on UDP port %d...\n", BROADCAST_PORT);

    /*  
     * Main loop: Wait for UDP messages  
     */  
    while (1)  
    {
        recv_len = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                            (struct sockaddr*)&client_addr, &addr_len);
        if (recv_len < 0)  
        {
            LOG_DEBUG("recvfrom\n");
            continue;
        }

        buffer[recv_len] = '\0';

        /*  
         * If the received message is NEED_IP, respond with own IP  
         */  
        if (strcmp(buffer, NEED_IP_MSG) == 0)  
        {
            own_ip = get_own_ip();
            sendto(sockfd, own_ip, strlen(own_ip), 0,
                   (struct sockaddr*)&client_addr, addr_len);
            LOG_DEBUG("Responded to %s with IP: %s\n", inet_ntoa(client_addr.sin_addr), own_ip);
            free(own_ip);
        }
    }

    close(sockfd);
    pthread_exit(NULL);
}

void clear_modbus_mapping(modbus_mapping_t *mb_mapping, uint8_t flags)
{
    if (!mb_mapping)
        return;

    if ((flags & CLEAR_BITS) && mb_mapping->tab_bits)
    {
        memset(mb_mapping->tab_bits, 0, mb_mapping->nb_bits * sizeof(uint8_t));
    }	
    
    if ((flags & CLEAR_INPUT_BITS) && mb_mapping->tab_input_bits)
    {
        memset(mb_mapping->tab_input_bits, 0, mb_mapping->nb_input_bits * sizeof(uint8_t));
    }
    
    if ((flags & CLEAR_REGISTERS) && mb_mapping->tab_registers)
    {
        memset(mb_mapping->tab_registers, 0, mb_mapping->nb_registers * sizeof(uint16_t));
    }
    
    if ((flags & CLEAR_INPUT_REGISTERS) && mb_mapping->tab_input_registers)
    {
        memset(mb_mapping->tab_input_registers, 0, mb_mapping->nb_input_registers * sizeof(uint16_t));
    }
}


/**
 * @brief Main function to initialize and manage Modbus and CAN communication.
 *
 * This function initializes the Modbus TCP server and the CAN interface.
 * It sets up necessary sockets, handles incoming Modbus requests from clients,
 * translates them into CAN messages, receives responses, maps them into Modbus
 * registers, and sends the response back to the client.
 *
 * It also creates a separate heartbeat thread to periodically send a CAN heartbeat
 * message. All operations continue in a loop to support real-time communication.
 *
 * @return Returns 0 on success, -1 on failure.
 */

int main()
{
    /*
     * Communication handles and structures
     */
    pthread_t             thread_id;
    pthread_t             ip_responder_id;
    int                   socket_fd;
    struct sockaddr_can   addr;
    struct ifreq          ifr;
    struct can_filter     rfilter;

    /*
     * Modbus related variables
     */
    modbus_t             *ctx;
    modbus_mapping_t     *mb_mapping;
    int                   server_socket;
    uint8_t               query[MAX_ADU_LENGTH];
    int                   client_socket;

    /*
     * Data processing variables
     */
    uint32_t              can_id;
    uint32_t              data_header;
    uint32_t              req_type;
    uint32_t              fun_code;
    uint32_t              total_size;
    uint16_t              start_addr;
    uint16_t              length;
    int                   found;
    device_data          *selected_array = NULL; 
    int                   tcp_found         = 0;
    int                   tcp_current_dataset_size;
    device_data           *tcp_selected_array = NULL;

    /*
     * Loop and index variables
     */
    int                   dataset_index;
    int                   bit;
    int                   data_index;
    int                   register_index;
    size_t                register_offset;
    const int             total_datasets = sizeof(all_datasets) / sizeof(all_datasets[0]);
    const int             tcp_total_datasets = sizeof(tcp_data) / sizeof(tcp_data[0]);
    int                   current_dataset_size;

    /*
     * Status variables
     */
    uint8_t               ret = 0;
    int                   rc;
    uint8_t               *write_value = received_data; 
    uint8_t               clear_flag   = CLEAR_NONE; 
    uint8_t               cp           = 0; 
 
    /*
     * EE_Prom variables
     */
    uint32_t               fd; 
    uint32_t               offset; 
    uint32_t               Read; 
    uint32_t               Write=0; 

    /*
     * Initialize Modbus TCP context
     */
    ctx = modbus_new_tcp("0.0.0.0", SERVER_PORT);
    if (!ctx)
    {
        LOG_ERROR("Failed to create Modbus context: %s\n", modbus_strerror(errno));
        return -1;
    }

    /*
     * Configure Modbus debugging
     */
    modbus_set_debug(ctx, 0);

    /*
     * Allocate Modbus register mapping
     * Special focus on input registers (5000 allocated)
     */
    mb_mapping = modbus_mapping_new(MODBUS_ALLOC_NUM_COILS,MODBUS_ALLOC_NUM_DISCRETE_INPUTS,
		                    MODBUS_ALLOC_NUM_HOLDING_REGISTERS,MODBUS_ALLOC_NUM_INPUT_REGISTERS);
    if (!mb_mapping)
    {
        LOG_ERROR("Failed to allocate Modbus registers: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    /*
     * Start Modbus TCP listener
     */
    server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket == -1)
    {
        LOG_ERROR("Failed to listen on Modbus TCP: %s\n", modbus_strerror(errno));
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return -1;
    }

    /*
     * Server startup message
     */
    LOG_DEBUG("Modbus TCP Server started on port %d\n", SERVER_PORT);

    /*
     * Initialize CAN interface
     */
    setup_can_interface(CAN_INTERFACE, CAN_BITRATE);

    /*
     * Create CAN socket
     */
    socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd < 0)
    {
        LOG_ERROR("Socket creation failed\n");
        return -1;
    }

    /*
     * Configure CAN interface
     */
    strcpy(ifr.ifr_name, CAN_INTERFACE);
    if (ioctl(socket_fd, SIOCGIFINDEX, &ifr) < 0)
    {
        LOG_ERROR("Error getting CAN interface index\n");
        return -1;
    }

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    
    /*
     * Bind CAN socket
     */
    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG_ERROR("Error binding socket to CAN interface\n");
        return -1;
    }

    rfilter.can_id = CAN_EFF_FLAG;             // Match only extended ID flag
    rfilter.can_mask = CAN_EFF_FLAG;           // Filter only by EFF flag
    
    /*
     * Set filter: accept only 29-bit CAN frames
     */ 
    if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0)
    {
       LOG_ERROR("Error setting CAN filter for Extended ID frames\n");
       return -1;
    }

    /*
     * Create heartbeat thread
     */
    if (pthread_create(&thread_id, NULL, heartbeat_thread, &socket_fd) != 0)
    {
        LOG_ERROR("Error creating heartbeat thread\n");
        return -1;
    }

    /*
     * Create IP responder thread
     */
    if (pthread_create(&ip_responder_id, NULL, ip_response_thread, NULL) != 0)
    {
        LOG_ERROR("Error creating IP responder thread\n");
        return -1;
    }

    /*
     * EE _prom mem pointer open with both read and write
     */
    fd = open(EEPROM_PATH, O_RDWR);
    if (fd < 0) {
        perror("[ERROR] Failed to open EEPROM");
        return -1;
    }


    /*
     * Main server loop
     */
    while (1)
    {
        /*
         * Accept new client connection
         */
        client_socket = modbus_tcp_accept(ctx, &server_socket);
        if (client_socket == -1)
        {
            LOG_ERROR("Failed to accept client: %s\n", modbus_strerror(errno));
            continue;
        }

        LOG_DEBUG("Client connected.\n");

        /*
         * Client communication loop
         */
        while (1)
        {

            /*
	     * Receive Modbus data from the client
	     */
            rc = modbus_receive(ctx, query);
	   
	    /* 
	     * Returns any error, close the connection and wait for new connection
	     */ 
	    if (rc == -1)
            {
                LOG_ERROR("Client disconnected.\n\n");
                close(client_socket);
                break;
            }
	    /*
	     * Receive Zero byte data, continue
	     */
	    else if(rc == 0)
            {
              continue;     
	    }		       
	    
	    log("\n\n"); 
	    LOG_DEBUG("New request coming from Modbus client.\n");
	    /*
             * Parse Modbus request parameters
             */
            start_addr  = ((query[8] << 8) | query[9]) + 1;
            length      = (query[10] << 8) | query[11];
            fun_code    = query[7];
	    /*
	     * Reset var
	     */  
	    tcp_found   = 0;
            found       = 0;
            offset      = 0;
            
	    /*
             * The received_data buffer is shared for both CAN read and write operations.
             */
            write_value = received_data;

            /*
             * Here we check whether the requested function code is a valid operation or not.
             */
            if ((fun_code == 0x03) || (fun_code == 0x04) || (fun_code == 0x10)) 
	    {
                Write = Read = length * 2;
            } 
	    else if (fun_code == 0x06) 
	    {
                Write = Read = 2;
            } 
	    else if ((fun_code == 0x01) || (fun_code == 0x02)) 
	    {
                Read = length;
            } 
	    else 
	    {
                LOG_ERROR("Function code not found: Illegal = %d\n", fun_code);

                /*
                 * Set error values in all register types.
                 */
                ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		if (ret == -1)
                {
                    LOG_ERROR("Failed to send Modbus exception response\n");
                }

                continue;
            }

            /*
             * Search through all TCP datasets for a matching register.
             * This dataset is for TCP configuration only — no CAN bus operations.
             */
            for (dataset_index = 0; dataset_index < tcp_total_datasets; dataset_index++) 
	    {
                tcp_selected_array = tcp_data[dataset_index];
                tcp_current_dataset_size = tcp_dataset_counts[dataset_index];
                 
		LOG_DEBUG("Processing TCP dataset %d out of %d total datasets.\n", dataset_index, tcp_current_dataset_size);

                if (!(tcp_selected_array[0].reg_address % 10000 <= start_addr) ||
                    !(tcp_selected_array[tcp_current_dataset_size - 1].reg_address % 10000 >= start_addr)) 
		{

                    for (data_index = 0; data_index < tcp_current_dataset_size; data_index++) 
		    {
                        offset += tcp_selected_array[data_index].size;
                    }
                    continue;
                }

                for (data_index = 0; data_index < tcp_current_dataset_size; data_index++) 
		{
                    offset += tcp_selected_array[data_index].size;

                    if (start_addr != (tcp_selected_array[data_index].reg_address % 10000)) 
		    
		    {
                        continue;
                    }

                    offset -= tcp_selected_array[data_index].size;

                    if (tcp_selected_array[data_index].fun_code[0] == fun_code ||
                        tcp_selected_array[data_index].fun_code[1] == fun_code ||
                        tcp_selected_array[data_index].fun_code[2] == fun_code) 
		    {   

                        tcp_found = 1;

                        /* Print matched register details */
                        LOG_DEBUG("Found TCP Register:\n");
                        LOG_DEBUG("  Name        : %s\n", tcp_selected_array[data_index].attribute_name);
                        LOG_DEBUG("  Address     : %u (0x%X)\n", tcp_selected_array[data_index].reg_address, tcp_selected_array[data_index].reg_address);
                        LOG_DEBUG("  Size        : %u\n", tcp_selected_array[data_index].size);
                        LOG_DEBUG("  Function(s) : %02X %02X %02X\n\n",
                                 tcp_selected_array[data_index].fun_code[0],
                                 tcp_selected_array[data_index].fun_code[1],
                                 tcp_selected_array[data_index].fun_code[2]);

                        break;
                    } 
		    else 
		    {   
	                /*
                         * Register address matches, but requested function code is not supported.
                         * Print debug information for diagnosis.
                         */
                        LOG_DEBUG("Register address matched (%u), but requested function code 0x%02X is not supported.\n",
                                                                                            tcp_selected_array[data_index].reg_address % 10000, fun_code);
                        LOG_DEBUG("Supported Function Codes: %02X %02X %02X\n\n",
                                                                                tcp_selected_array[data_index].fun_code[0],
                                                                                tcp_selected_array[data_index].fun_code[1],
                                                                                tcp_selected_array[data_index].fun_code[2]);
                        break;
                    }
                }

                if (tcp_found)
                    break;
            }
            
	    log("\n"); 
            
            /* 
             * If we found a matching register from the TCP dataset    
             */
            if (tcp_found)
            {
                
	        LOG_DEBUG("Received a TCP module configuration request.\n");

	       /*
                * If the requested size is greater than available size,
                * calculate the remaining dataset size from the matched point.
                * If the user requests more than the available size, it is invalid.
                * Return an error.
                */
                
		total_size = 0;
                for (data_index = data_index; data_index < tcp_current_dataset_size; data_index++)
                {
                    total_size += tcp_selected_array[data_index].size;
                }

                /*
                 * If the requested size is greater than available size,
                 * return an error
                 */
                if (total_size < Read)
                {
                    LOG_ERROR("Register address found, but requested size (%d bytes) exceeds available dataset size (%d bytes)\n", Read, total_size);

                    /*
                     * Set error values in all register types
                     */
                    ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
		    if (ret == -1)
                    {
                       LOG_ERROR("Failed to send Modbus exception response\n");
                    }
                    continue;
                }

                /*
                 * Move the file pointer to the correct offset where the register data is stored
                 */
                ret = lseek(fd, offset, SEEK_SET);
                if (ret == -1)
                {
                    LOG_ERROR("lseek failed: %s\n", strerror(errno));
                    ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
		    if (ret == -1)
                    {
                       LOG_ERROR("Failed to send Modbus exception response\n");
                    }
                    continue;
                }

                /*
                 * Write operation supported for function codes:
                 * 0x06 = Single Register, 0x10 = Multiple Registers
                 */
                if ((fun_code == 0x06) || (fun_code == 0x10))
                {
                    LOG_DEBUG("EEPROM Write operation detected: Configartion EE_prome Function Code = 0x%02X\n", fun_code);

                    if (fun_code == 0x06)
                    {
                        /*
                         * Extract single register value from query
                         */
                        write_value[0] = query[10];
                        write_value[1] = query[11];

                        /*
                         * Write the value to EEPROM at the correct offset
                         */
                        ret = write(fd, write_value, Write);
                        if (ret != Write)
                        {
                            LOG_ERROR("Failed to write single register to EEPROM (expected %d bytes, wrote %d)\n", Write, ret);
                            ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
			    if (ret == -1)
                            {
                                LOG_ERROR("Failed to send Modbus exception response\n");
                            }
                            continue;
                        }
                    }
                    else
                    {
                        for (cp = 0; cp < Write; cp++)
                        {
                            write_value[cp] = query[cp + 13];
                        }

                        /*
                         * Write multiple values to EEPROM at the correct offset
                         */
                        ret = write(fd, write_value, Write);
                        if (ret != Write)
                        {
                            LOG_ERROR("Failed to write multiple registers to EEPROM (expected %d bytes, wrote %d)\n", Write, ret);
                            ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
			    if (ret == -1)
                            {
                                LOG_ERROR("Failed to send Modbus exception response\n");
                            } 
                            continue;
                        }
                    }

                    LOG_DEBUG("EEPROM write operation successful\n");

                    /*
                     * If both lseek and write were successful, go to the reply label to send response
                     */
                    goto EE_PROM_write_reply;
                }

		LOG_DEBUG("EEPROM Read operation detected: Configartion EE_prome Function Code = 0x%02X\n", fun_code);

                /*
                 * Read number of bytes from the file into received_data buffer
                 */
                ret = read(fd, received_data, Read);
                if (ret != Read)
                {
                    LOG_ERROR("EEPROM read failed or incomplete (expected %d, got %d): %s\n", Read, ret, strerror(errno));
                    ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_SLAVE_OR_SERVER_FAILURE);
		    if (ret == -1)
                    {
                        LOG_ERROR("Failed to send Modbus exception response\n");
                    }
                    continue;
                }

                LOG_DEBUG("EEPROM read operation successful\n");

                /*
                 * If both lseek and read were successful, go to the reply label to send response
                 */
                goto EE_PROM_Read_reply;
            }
            
	    /*
             * Search through all datasets for a matching register.
             * This dataset is for CAN module read/write operation support.
             */
            for (dataset_index = 0; dataset_index < total_datasets; dataset_index++)
            {
                selected_array = all_datasets[dataset_index];
                data_header = header[dataset_index].data_header;
                current_dataset_size = dataset_counts[dataset_index];

                LOG_DEBUG("Processing CAN dataset %d out of %d total datasets.\n", dataset_index, current_dataset_size);

                if (!(selected_array[0].reg_address % 10000 <= start_addr) ||
                    !(selected_array[current_dataset_size - 1].reg_address % 10000 >= start_addr))
                    continue;

                for (data_index = 0; data_index < current_dataset_size; data_index++)
                {
                    if (start_addr != (selected_array[data_index].reg_address % 10000))
                        continue;

                    if (selected_array[data_index].fun_code[0] == fun_code ||
                        selected_array[data_index].fun_code[1] == fun_code ||
                        selected_array[data_index].fun_code[2] == fun_code)
                    {
                        found = 1;

                        /* Print matched CAN register details */
                        LOG_DEBUG("Found CAN Register:\n");
                        LOG_DEBUG("  Name        : %s\n", selected_array[data_index].attribute_name);
                        LOG_DEBUG("  Address     : %u (0x%X)\n", selected_array[data_index].reg_address, selected_array[data_index].reg_address);
                        LOG_DEBUG("  Size        : %u\n", selected_array[data_index].size);
                        LOG_DEBUG("  Function(s) : %02X %02X %02X\n",
                                 selected_array[data_index].fun_code[0],
                                 selected_array[data_index].fun_code[1],
                                 selected_array[data_index].fun_code[2]);

                        break;
                    }
                    else
                    {
			/*
                         * Register address matches, but requested function code is not supported.
                         * Print debug information for diagnosis.
                         */
                        LOG_DEBUG("Register address matched (%u), but requested function code 0x%02X is not supported.\n",
                                                                                            selected_array[data_index].reg_address % 10000, fun_code);
                        LOG_DEBUG("Supported Function Codes: %02X %02X %02X\n\n",
                                                                                 selected_array[data_index].fun_code[0],
                                                                                 selected_array[data_index].fun_code[1],
                                                                                 selected_array[data_index].fun_code[2]);
                        break;
                    }
                }

                if (found)
                    break;
            } 
            
	    /*
             * Handle case when register not found
	     * Return Error message to the modbus client
	     */
            
            if (!found)
            {
                LOG_ERROR("Register address %u not found in any dataset\n", start_addr);

                /*
                 * Set error values in all register types
                 */
                ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
                if (ret == -1)
                {
                    LOG_ERROR("Failed to send Modbus exception response\n");
                }
                continue;
            }
            
	    /*
             * If the requested size is greater than available size,
             * calculate the remaining dataset size from the matched point.
             * If the user requests more than the available size, it is invalid.
             * Return an error.
             */
            
	    total_size = 0;
            for (data_index = data_index; data_index < current_dataset_size; data_index++)
            {
                total_size += selected_array[data_index].size;
            }

	    /*
	     * if the requested size is greater than avail size,
	     * return error
             */
            if (total_size < Read)
            {
                LOG_ERROR("Register address found, but requested size (%d bytes) exceeds available dataset size (%d bytes)\n", Read, total_size);

                /*
                 * Set error values in all register types
                 */
                ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS);
                if (ret == -1)
                {
                    LOG_ERROR("Failed to send Modbus exception response\n");
                }
                continue;
            }
            
            LOG_DEBUG("Found data at index: %d\n", register_index);

            LOG_DEBUG("Requested data size: %d bytes\n\n", length);

            /*
             * This is CAN Module write operation.
             * If the function code matches, a write operation will happen.
             * Supported function codes: 0x06 & 0x10.
             */
            if ((fun_code == 0x06) || (fun_code == 0x10))
            {
                LOG_DEBUG("Write operation detected: For CAN module, Function Code = 0x%02X\n", fun_code);

                /*
                 * Prepare CAN message
                 */
                req_type = TCP_TO_ETU_WRITE_REQ_ID;

                /*
                 * Construct the CAN ID
                 */
                can_id = (0 << 27) |
                         (1 << 23) |
                         (data_header << 20) |
                         (req_type << 16) |
                         (start_addr);

                if (fun_code == 0x06)
                {
                    /*
                     * Extract single register value from query
                     */
                    length = 1;
                    write_value[0] = query[10];
                    write_value[1] = query[11];

                    /*
                     * Transmit CAN Write Request and handle response
                     */
                    ret = can_txrx_reassemble_frag_data_write(socket_fd, can_id, write_value, length);
                    if (ret != 0)
                    {
                        LOG_ERROR("CAN communication failed\n\n");

                        ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_GATEWAY_TARGET);
                        if (ret == -1)
                        {
                            LOG_ERROR("Failed to send Modbus exception response\n");
                        }
                        continue;
                    }
                }
                else
                {
                    for (cp = 0; cp < Write; cp++)
                    {
                        write_value[cp] = query[cp + 13];
                    }

                    /*
                     * Transmit CAN Write Request and handle response
                     */
                    ret = can_txrx_reassemble_frag_data_write(socket_fd, can_id, write_value, length);
                    if (ret != 0)
                    {
                        LOG_ERROR("CAN communication failed\n\n");

                        ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_GATEWAY_TARGET);
                        if (ret == -1)
                        {
                            LOG_ERROR("Failed to send Modbus exception response\n");
                        }
                        continue;
                    }
                }

                LOG_DEBUG("CAN module write operation successful\n");

                /*
                 * Go to the reply label to send response
                 */
                goto Can_write_okey_riply;
            }

            /*
             * Here: CAN module read operation will be detected
             */
            LOG_DEBUG("Read operation detected: For CAN module, Function Code = 0x%02X\n", fun_code);

            /*
             * Prepare CAN message
             */
            req_type = CAN_READ_REQ_MSG_ID;

            /*
             * Construct the CAN ID
             */
            can_id = (0 << 27) |
                     (1 << 23) |
                     (data_header << 20) |
                     (req_type << 16) |
                     (start_addr);

            /*
             * Send CAN request and receive response
             */
            ret = can_txrx_reassemble_frag_data_read(socket_fd, can_id, length, fun_code);
            if (ret != 0)
            {
                LOG_ERROR("CAN communication failed\n\n");

                ret = modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_GATEWAY_TARGET);
                if (ret == -1)
                {
                    LOG_ERROR("Failed to send Modbus exception response\n");
                }
                continue;
            }

            LOG_DEBUG("CAN module read operation successful\n");

            /*
             * Process received CAN data into Modbus registers
             */

EE_PROM_Read_reply:	      
             start_addr--;
             clear_flag = CLEAR_NONE;
	     /*
	      * Read Coils
	      */
	     if (fun_code == MODBUS_FUNC_READ_COILS)  
              {
		     clear_flag = CLEAR_BITS;
                     for (register_offset = start_addr, data_index = 0 , bit = 0; register_offset < length + start_addr; register_offset++)
                      {
                          mb_mapping->tab_bits[register_offset] = ((received_data[data_index] >> bit++) & 0x01 );

			  if(bit >= 7)
			  {
                            bit=0;
			    data_index++;
			  }
                      }
              }
	     /*
	      * Read Discrete Inputs
	      */
             else if (fun_code == MODBUS_FUNC_READ_DISCRETE_INPUTS)  
	     {       
		     clear_flag = CLEAR_INPUT_BITS;
                     for (register_offset = start_addr, data_index = 0 , bit = 0; register_offset < length + start_addr; register_offset++)
                      {
                          mb_mapping->tab_input_bits[register_offset] = ((received_data[data_index] >> bit++) & 0x01 );
		           
		          if(bit >= 7)
			   {
			     bit=0;
			     data_index++; 
			   }			  
                      }
             }
	     /*
	      * Read Holding Registers
	      */
             else if (fun_code == MODBUS_FUNC_READ_HOLDING_REGISTERS)  
              {
		     clear_flag = CLEAR_REGISTERS;
                     for (register_offset = start_addr, data_index = 0; register_offset < length + start_addr; register_offset++, data_index++)
                       {
                          mb_mapping->tab_registers[register_offset] = ((uint16_t)received_data[2 * data_index] << 8) |
                                                                       received_data[2 * data_index + 1];
                       }
              }
	      /*
	       * Read Input Registers
	       */
             else if (fun_code == MODBUS_FUNC_READ_INPUT_REGISTERS)  
               {
		     clear_flag = CLEAR_INPUT_REGISTERS;  
                     for (register_offset = start_addr, data_index = 0; register_offset < length + start_addr; register_offset++, data_index++)
                       {
                         mb_mapping->tab_input_registers[register_offset] = ((uint16_t)received_data[2 * data_index] << 8) |
                                                                            received_data[2 * data_index + 1];
                       }       
               } 

Can_write_okey_riply:	   
EE_PROM_write_reply:	     
             /*
              * Send Modbus response to client
              */
             ret = modbus_reply(ctx, query, rc, mb_mapping);

             if(ret == -1)
             {
                LOG_ERROR("Server to client response failed: %s\n\n", modbus_strerror(errno));
             }
             else
             {
                LOG_DEBUG("Server to client response succeeded\n\n");
             }
               
	    clear_modbus_mapping(mb_mapping,clear_flag);
	}

     }

    /*
     * Cleanup before exit
     */
    close(socket_fd);
    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);
    return 0;

}

