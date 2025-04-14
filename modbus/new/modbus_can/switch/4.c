#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/netdevice.h>
#include <linux/sockios.h>
#include <linux/net_switch_config.h>

int configure_switch(struct net_switch_config *cmd_struct, const char *iface_name) {
    struct ifreq ifr;
    int sockfd;

    // Open the socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Can't open the socket");
        return -1;
    }

    // Set interface name
    strncpy(ifr.ifr_name, iface_name, IFNAMSIZ);
    ifr.ifr_data = (char*)cmd_struct;

    // Send ioctl command
    if (ioctl(sockfd, SIOCSWITCHCONFIG, &ifr) < 0) {
        perror("Command failed");
        close(sockfd);
        return -1;
    }

    printf("Command success\n");
    close(sockfd);
    return 0;
}

int main(void) {
    struct net_switch_config cmd_struct;
    
    // Example for adding a multicast address
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_ADD_MULTICAST;
    // Set the multicast MAC address (example: LLDP address)
    memcpy(cmd_struct.addr, "\x01\x80\xc2\x00\x00\x0e", 6);  // LLDP multicast address
    cmd_struct.port = 0x03;  // Port 0 and 1
    cmd_struct.vid = 100;    // VLAN ID
    cmd_struct.super = 1;    // Enable super mode

    if (configure_switch(&cmd_struct, "eth1") < 0) {
        return -1;
    }

    // Example for deleting a multicast address
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_DEL_MULTICAST;
    memcpy(cmd_struct.addr, "\x01\x80\xc2\x00\x00\x0e", 6);  // LLDP multicast address
    cmd_struct.vid = 100;    // VLAN ID

    if (configure_switch(&cmd_struct, "eth0") < 0) {
        return -1;
    }

    // Example for adding a VLAN
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_ADD_VLAN;
    cmd_struct.vid = 200;  // VLAN ID
    cmd_struct.port = 0x03;  // Port 0 and 1
    cmd_struct.untag_port = 0x01;  // Port 0 untagged
    cmd_struct.reg_multi = 0x03;  // Multicast registered ports
    cmd_struct.unreg_multi = 0x01;  // Multicast unregistered ports

    if (configure_switch(&cmd_struct, "eth0") < 0) {
        return -1;
    }

    // Example for deleting a VLAN
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_DEL_VLAN;
    cmd_struct.vid = 200;  // VLAN ID

    if (configure_switch(&cmd_struct, "eth0") < 0) {
        return -1;
    }

    // Example for setting the port state
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_SET_PORT_STATE;
    cmd_struct.port = 1;  // Port 1
    cmd_struct.port_state = PORT_STATE_FORWARD;  // Forward state

    if (configure_switch(&cmd_struct, "eth0") < 0) {
        return -1;
    }

    // Example for getting port state
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_GET_PORT_STATE;
    cmd_struct.port = 1;  // Port 1

    if (configure_switch(&cmd_struct, "eth0") < 0) {
        return -1;
    }
    printf("Port State: %d\n", cmd_struct.port_state);

    // Example for rate limit configuration
    memset(&cmd_struct, 0, sizeof(cmd_struct));
    cmd_struct.cmd = CONFIG_SWITCH_RATELIMIT;
    cmd_struct.direction = 1;  // Transmit
    cmd_struct.port = 0;       // Port 0
    cmd_struct.bcast_rate_limit = 1000;  // 1000 packets/sec
    cmd_struct.mcast_rate_limit = 500;   // 500 packets/sec

    if (configure_switch(&cmd_struct, "eth0") < 0) {
        return -1;
    }

    return 0;
}

