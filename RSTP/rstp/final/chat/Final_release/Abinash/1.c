#include <stdio.h>

int main() {
    char *a = "AM437x";
    char *b = "192.168.0.1"; 
    char *c = "8001";
    char *d = "forwarding";
    char *e = "8002";
    char *f = "forwarding";

    // Declare a buffer to hold the formatted string
    char buffer[256];

    // Format the string and store it in the buffer
    snprintf(buffer, sizeof(buffer), "%-13s IP: %-20s Port %s - State: %-20s Port %s - State: %s\n", a, b, c, d, e, f);

    // Print the formatted buffer
    printf("\n\n%s\n\n", buffer);

    return 0;
}

