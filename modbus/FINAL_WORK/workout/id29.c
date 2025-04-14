#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>

#define CAN_INTERFACE "can0"

void setup_can_interface() {
    system("ip link set " CAN_INTERFACE " down");
    system("ip link set " CAN_INTERFACE " type can bitrate 125000");
    system("ip link set " CAN_INTERFACE " up");
}

int send_can_frame() {
    int sock;
    struct sockaddr_can addr;
    struct ifreq ifr;
    struct can_frame frame;
    
    // Create socket
    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0) {
        perror("Socket error");
        return -1;
    }

    strcpy(ifr.ifr_name, CAN_INTERFACE);
    ioctl(sock, SIOCGIFINDEX, &ifr);

    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind error");
        close(sock);
        return -1;
    }

    // Setup CAN frame
    frame.can_id = 0x1200333 | CAN_EFF_FLAG; // 29-bit ID
    frame.can_dlc = 8;
    frame.data[0] = 0xDE;
    frame.data[1] = 0xAD;
    frame.data[2] = 0xBE;
    frame.data[3] = 0xEF;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;

    if (write(sock, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("Write error");
        close(sock);
        return -1;
    }

    printf("CAN frame sent successfully\n");
    close(sock);
    return 0;
}

int main() {
    setup_can_interface();
    return send_can_frame();
}
