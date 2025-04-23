#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define MAX_CMD_LEN 128
#define MAX_LINE_LEN 256

typedef struct {
    char ip[64];
} PingArgs;

void print_timestamp() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);
    printf("[%s] ", time_str);
}

void *ping_thread(void *arg) {
    PingArgs *pingArgs = (PingArgs *)arg;
    char cmd[MAX_CMD_LEN];
    snprintf(cmd, sizeof(cmd), "ping -c 1 -W 1 %s", pingArgs->ip);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        print_timestamp();
        printf("Error running ping for %s\n", pingArgs->ip);
        pthread_exit(NULL);
    }

    char line[MAX_LINE_LEN];
    int reachable = 0;
    double rtt = 0.0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, "time=")) {
            reachable = 1;
            sscanf(strstr(line, "time="), "time=%lf", &rtt);
        }
    }

    pclose(fp);

    print_timestamp();
    if (reachable) {
        printf("Ping to %s successful. RTT = %.2f ms\n", pingArgs->ip, rtt);
    } else {
        printf("Ping to %s not reachable within 1.5 seconds.\n", pingArgs->ip);
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <IP1> <IP2>\n", argv[0]);
        return 1;
    }

    PingArgs ip1_args, ip2_args;
    strncpy(ip1_args.ip, argv[1], sizeof(ip1_args.ip));
    strncpy(ip2_args.ip, argv[2], sizeof(ip2_args.ip));

    while (1) {
        pthread_t t1, t2;

        pthread_create(&t1, NULL, ping_thread, &ip1_args);
        pthread_create(&t2, NULL, ping_thread, &ip2_args);

        pthread_join(t1, NULL);
        pthread_join(t2, NULL);
        usleep(200000); // sleep for 200 milliseconds (200,000 microseconds)
       // sleep(1);  // wait 1 second before next round
    }

    return 0;
}
