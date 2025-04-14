#include <stdio.h>
#include <linux/net_switch_config.h>
#include <linux/netdevice.h>
#include <linux/sockios.h>
#include <stdio.h>
#include <string.h>        // For strncpy(), memset()
#include <sys/socket.h>    // For socket(), SOCK_DGRAM, AF_INET
//#include <net/if.h>        // For IFNAMSIZ
#include <unistd.h>        // For close()
#include <sys/ioctl.h> 
#include <errno.h>

int main(void)
{
    struct net_switch_config cmd_struct;
    struct ifreq ifr;
    int sockfd;
    strncpy(ifr.ifr_name, "eth1", IFNAMSIZ);
    ifr.ifr_data = (char*)&cmd_struct;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Can't open the socket\n");
        return -1;
    }
    memset(&cmd_struct, 0, sizeof(struct net_switch_config));


    if (ioctl(sockfd, SIOCSWITCHCONFIG, &ifr) < 0) {
	      perror("ioctl");
	      printf("Command failed, errno: %d\n", errno);
        printf("Command failed\n");
        close(sockfd);
        return -1;
    }
    printf("command success\n");
    close(sockfd);
    return 0;
}
