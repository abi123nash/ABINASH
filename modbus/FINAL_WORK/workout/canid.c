#include <stdio.h>
#include <stdint.h>

int main() {
    // Defining the values based on the provided example
    uint32_t Module_Address = 0x00;  // 2-bit
    uint32_t Module_ID = 0x01;       // 4-bit
    uint32_t Data_Header = 0x02;     // 3-bit
    uint32_t Message_Type = 0x00;    // 4-bit
    uint32_t Data_ID = 0x0333;       // 16-bit

    // Corrected Module_ID shift (24 bits instead of 23)
    uint32_t can_id = (Module_Address << 27) | 
                      (Module_ID << 24) |  // Corrected shift
                      (Data_Header << 20) |
                      (Message_Type << 16) |
                      (Data_ID);

    // Print the computed CAN ID
    printf("Computed CAN ID: 0x%08X\n", can_id);

    return 0;
}

