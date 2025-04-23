#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>

#define PACKET_SIZE 64
#define TEST_DURATION 10 // Test duration in seconds

// Function to calculate checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *) buf;

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <IP_ADDRESS>\n", argv[0]);
        return 1;
    }

    char *target_ip = argv[1];
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_addr.s_addr = inet_addr(target_ip);

    struct timeval start_time, end_time, current_time;
    gettimeofday(&start_time, NULL);

    int packet_count = 0;
    long total_bytes = 0;

    while (1) {
        struct icmphdr icmp_packet;
        memset(&icmp_packet, 0, sizeof(icmp_packet));
        icmp_packet.type = ICMP_ECHO;
        icmp_packet.un.echo.id = getpid();
        icmp_packet.un.echo.sequence = packet_count++;
        icmp_packet.checksum = checksum(&icmp_packet, sizeof(icmp_packet));

        if (sendto(sockfd, &icmp_packet, sizeof(icmp_packet), 0, 
                   (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
            perror("Send failed");
            return 1;
        }

        char buffer[PACKET_SIZE];
        struct sockaddr_in response_addr;
        socklen_t addr_len = sizeof(response_addr);

        if (recvfrom(sockfd, buffer, sizeof(buffer), 0, 
                     (struct sockaddr *)&response_addr, &addr_len) > 0) {
            total_bytes += PACKET_SIZE;
        }

        gettimeofday(&current_time, NULL);
        double elapsed_time = (current_time.tv_sec - start_time.tv_sec) +
                              (current_time.tv_usec - start_time.tv_usec) / 1e6;
        if (elapsed_time >= TEST_DURATION)
            break;
    }

    gettimeofday(&end_time, NULL);
    double total_time = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_usec - start_time.tv_usec) / 1e6;

    double speed_mbps = (total_bytes / (1024.0 * 1024.0)) / total_time;
    printf("\nTest completed!\n");
    printf("Total Data Transferred: %.2f MB\n", total_bytes / (1024.0 * 1024.0));
    printf("Average Speed: %.2f MB/s\n", speed_mbps);

    close(sockfd);
    return 0;
}

