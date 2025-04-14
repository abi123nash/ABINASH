#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
//#include <arpa/inet.h>
//#include <linux/if_arp.h>

#define __RECV__

#define DEST_MAC_ADDR "ff:ff:ff:ff:ff:ff" // Broadcast MAC address
#define SRC_MAC_ADDR "00:bb:cc:dd:ee:ff" // Example source MAC address
#define ETHERTYPE_ARP 0x0806 // ARP EtherType
#define INTERFACE "eth0"
#define SRC_IP "192.168.0.1" // Example source IP
#define DEST_IP "192.168.0.241" // Example destination IP

#if 1
struct arp_header1 {
    uint16_t htype;   // Hardware type
    uint16_t ptype;   // Protocol type
    uint8_t hlen;     // Hardware address length
    uint8_t plen;     // Protocol address length
    uint16_t opcode;  // Operation (1 for request, 2 for reply)
    uint8_t sender_mac[6];
    uint8_t sender_ip[4];
    uint8_t target_mac[6];
    uint8_t target_ip[4];
};
#endif

int main() {
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_ll socket_address;
    unsigned char buffer[ETH_FRAME_LEN];
    int len;
    // Create a raw socket
    sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETHERTYPE_ARP));
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Get the interface index
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, INTERFACE, IFNAMSIZ - 1);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0) {
        perror("Failed to get interface index");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    int ifindex = ifr.ifr_ifindex;

    // Fill the Ethernet frame
    struct ether_header *eth_header = (struct ether_header *) buffer;
    memcpy(eth_header->ether_shost, ether_aton(SRC_MAC_ADDR)->ether_addr_octet, ETH_ALEN);
    memcpy(eth_header->ether_dhost, ether_aton(DEST_MAC_ADDR)->ether_addr_octet, ETH_ALEN);
    eth_header->ether_type = htons(ETHERTYPE_ARP);

    // Fill the ARP header
    struct arp_header1 *arp_hdr = (struct arp_header1 *)(buffer + sizeof(struct ether_header));
    arp_hdr->htype = htons(1); // Ethernet
    arp_hdr->ptype = htons(0x0800); // IPv4
    arp_hdr->hlen = ETH_ALEN; // MAC address length
    arp_hdr->plen = 4; // IPv4 address length
    arp_hdr->opcode = htons(1); // ARP request
    memcpy(arp_hdr->sender_mac, ether_aton(SRC_MAC_ADDR)->ether_addr_octet, ETH_ALEN);
    inet_pton(AF_INET, SRC_IP, arp_hdr->sender_ip);
    memset(arp_hdr->target_mac, 0x00, ETH_ALEN); // Target MAC unknown
    inet_pton(AF_INET, DEST_IP, arp_hdr->target_ip);

    // Total frame size
    size_t frame_len = sizeof(struct ether_header) + sizeof(struct arp_header1);

    // Bind the socket to the interface
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_ifindex = ifindex;
    socket_address.sll_halen = ETH_ALEN;
    memcpy(socket_address.sll_addr, eth_header->ether_dhost, ETH_ALEN);

    // Send the Ethernet frame
    if (sendto(sockfd, buffer, frame_len, 0, (struct sockaddr *) &socket_address, sizeof(socket_address)) < 0) {
        perror("Failed to send frame");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Raw Ethernet ARP frame sent successfully\n");
#ifdef __RECV__
    memset(buffer, 0x00, sizeof(buffer));

    len = sizeof(socket_address);
    // Send the Ethernet frame
    if (recvfrom(sockfd, buffer, frame_len, 0, (struct sockaddr *) &socket_address, &len) < 0) {
        perror("Failed to send frame");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    arp_hdr = (struct arp_header1 *)(buffer + sizeof(struct ether_header));

    printf("Raw Ethernet ARP frame received successfully = %x \n", arp_hdr->ptype);
#endif
    close(sockfd);
    return 0;
}

