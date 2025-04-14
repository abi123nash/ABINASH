#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <net/if.h>
#include <linux/netdevice.h>
#include <linux/sockios.h>
#include <linux/net_switch_config.h>

int main(void)
{
    struct net_switch_config cmd_struct;
    struct ifreq ifr;
    int sockfd;

    // Create a socket to interact with the network device
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Can't open the socket");
        return -1;
    }

    // Set the interface name (e.g., eth0)
    strncpy(ifr.ifr_name, "eth1", IFNAMSIZ);
    ifr.ifr_data = (char*)&cmd_struct;

    // Initialize the cmd_struct to 0
    memset(&cmd_struct, 0, sizeof(struct net_switch_config));

    // Configure to add a multicast address
    cmd_struct.cmd = CONFIG_SWITCH_ADD_MULTICAST; // Command for adding multicast address
    cmd_struct.addr[0] = 0x01;  // Set the multicast MAC address (example)
    cmd_struct.addr[1] = 0x80;
    cmd_struct.addr[2] = 0xC2;
    cmd_struct.addr[3] = 0x00;
    cmd_struct.addr[4] = 0x00;
    cmd_struct.addr[5] = 0x33;  // Multicast MAC Address: 01:80:C2:00:00:33

    cmd_struct.port = 0x01;      // Example: Slave 0 (Port 1)
    cmd_struct.vid = 100;        // VLAN ID 100
    cmd_struct.super = 1;        // Super Flag (set to 1)

    // Perform the ioctl to add the multicast address
    if (ioctl(sockfd, SIOCSWITCHCONFIG, &ifr) < 0) {
        perror("Command failed to add multicast address");
        close(sockfd);
        return -1;
    }

    printf("Multicast address added successfully\n");

    // Now, let's delete the multicast address (using CONFIG_SWITCH_DEL_MULTICAST)
    memset(&cmd_struct, 0, sizeof(struct net_switch_config));

    cmd_struct.cmd = CONFIG_SWITCH_DEL_MULTICAST; // Command for deleting multicast address
    cmd_struct.addr[0] = 0x01;  // Set the same multicast MAC address as before
    cmd_struct.addr[1] = 0x80;
    cmd_struct.addr[2] = 0xC2;
    cmd_struct.addr[3] = 0x00;
    cmd_struct.addr[4] = 0x00;
    cmd_struct.addr[5] = 0x33;  // Same MAC address as the one we added

    cmd_struct.vid = 100;        // VLAN ID 100 (same as before)

    // Perform the ioctl to delete the multicast address
    if (ioctl(sockfd, SIOCSWITCHCONFIG, &ifr) < 0) {
        perror("Command failed to delete multicast address");
        close(sockfd);
        return -1;
    }

    printf("Multicast address deleted successfully\n");

    // Close the socket
    close(sockfd);
    return 0;
}

