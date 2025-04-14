#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#define CAN_INTERFACE    "can0"
#define TRIGGER_ID       0x01200333 
#define START_ID         0x01210333 
#define ACK_BASE_ID      0x01290333 
#define FRAME_COUNT      12
#define DATA_SIZE        56

u_int8_t static_data[DATA_SIZE];

void initialize_can_data() {
        
	for (int i = 0; i < DATA_SIZE; i++) 
	{
            static_data[i] = 0x10 + i;
        }
}


unsigned short crc16(const unsigned char *data, int length) {
    unsigned short crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

void send_can_frame(int sock, int id, unsigned char *data) {
    struct can_frame frame;
    frame.can_id = id | CAN_EFF_FLAG;
    frame.can_dlc = 8;
    memcpy(frame.data, data, 8);
    write(sock, &frame, sizeof(struct can_frame));
    printf("Sent frame ID: 0x%X, Data: ", id);
    for (int i = 0; i < 8; i++) printf("%02X ", frame.data[i]);
    printf("\n");
}

int main() {
    int sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_frame frame;
    int i;
   
    initialize_can_data();
    
    strcpy(ifr.ifr_name, CAN_INTERFACE);
    ioctl(sock, SIOCGIFINDEX, &ifr);
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    
    printf("Listening for CAN frame 0x%X...\n", TRIGGER_ID);
    while (1) {
        if (read(sock, &frame, sizeof(struct can_frame)) > 0) {
            if (frame.can_id == (TRIGGER_ID | CAN_EFF_FLAG) ) 
	    {
                printf("Trigger frame received! Starting transmission...\n");
                
                for (i = 0; i < FRAME_COUNT; i++) {
                    unsigned char send_data[8] = {0};
                    send_data[0] = i + 1; // Frame number
                    memcpy(&send_data[1], &static_data[i * 5], 5);
                    unsigned short crc = crc16(send_data, 6);
                    send_data[6] = (crc >> 8) & 0xFF;
                    send_data[7] = crc & 0xFF;
                    
                    int frame_id = START_ID + (i * 5);
                    send_can_frame(sock, frame_id, send_data);
                    
                    while (1) {
                        if ( (read(sock, &frame, sizeof(struct can_frame)) > 0) &&  (frame.can_id == ((ACK_BASE_ID | CAN_EFF_FLAG) + (i * 5))) ) {
                            printf("Received ACK for frame %d\n", i + 1);
                            break;
                        }
                    }
                }
            }
        }
    }
    close(sock);
    return 0;
}
