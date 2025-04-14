#include <modbus/modbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SERVER_ID     1
#define MODBUS_PORT   1502

int registers[500]={0};

void initialize_registers() {
    // Current Metering Data
    registers[1] = 100;   // R Phase Current
    registers[3] = 200;   // Y Phase Current
    registers[5] = 300;   // B Phase Current
    registers[7] = 400;   // N Phase Current
    registers[9] = 500;   // G Phase Current
    registers[11] = 600;  // Reserved
    registers[13] = 700;  // Reserved
    registers[15] = 800;  // R Phase Current Max
    registers[17] = 900;  // Y Phase Current Max
    registers[19] = 1000; // B Phase Current Max
    registers[21] = 1100; // N Phase Current Max
    registers[23] = 1200; // G Phase Current Max
    registers[25] = 1300; // Reserved
    registers[27] = 1400; // Reserved
    registers[29] = 1500; // R Phase Current Max Time Stamp
    registers[33] = 1600; // Y Phase Current Max Time Stamp
    registers[37] = 1700; // B Phase Current Max Time Stamp
    registers[41] = 1800; // N Phase Current Max Time Stamp
    registers[45] = 1900; // G Phase Current Max Time Stamp
    registers[49] = 2000; // Reserved
    registers[53] = 2100; // Reserved
    registers[57] = 2200; // R Phase Current Min
    registers[59] = 2300; // Y Phase Current Min
    registers[61] = 2400; // B Phase Current Min
    registers[63] = 2500; // N Phase Current Min
    registers[65] = 2600; // G Phase Current Min
    registers[67] = 2700; // Reserved
    registers[69] = 2800; // Reserved
    registers[71] = 2900; // R Phase Current Min Time Stamp
    registers[75] = 3000; // Y Phase Current Min Time Stamp
    registers[79] = 3100; // B Phase Current Min Time Stamp
    registers[83] = 3200; // N Phase Current Min Time Stamp
    registers[87] = 3300; // G Phase Current Min Time Stamp
    registers[91] = 3400; // Reserved
    registers[95] = 3500; // Reserved
    registers[99] = 3600; // Average Current
    registers[101] = 3700; // R Phase % Load Current
    registers[102] = 3800; // Y Phase % Load Current
    registers[103] = 3900; // B Phase % Load Current
    registers[104] = 4000; // N Phase % Load Current
    registers[105] = 4100; // Current unbalance R Phase
    registers[106] = 4200; // Current unbalance Y Phase
    registers[107] = 4300; // Current unbalance B Phase
    registers[108] = 4400; // Current unbalance Max
    registers[109] = 4500; // THD Current R
    registers[110] = 4600; // THD Current Y
    registers[111] = 4700; // THD Current B
    registers[112] = 4800; // Total THD Current
    registers[113] = 4900; // R Phase Demand Current
    registers[115] = 5000; // Y Phase Demand Current
    registers[117] = 5100; // B Phase Demand Current
    registers[119] = 5200; // N Phase Demand Current
    registers[121] = 5300; // R Phase Demand Current Max
    registers[123] = 5400; // Y Phase Demand Current Max
    registers[125] = 5500; // B Phase Demand Current Max
    registers[127] = 5600; // N Phase Demand Current Max
    registers[129] = 5700; // R Phase Demand Current Max TimeStamp
    registers[133] = 5800; // Y Phase Demand Current Max TimeStamp
    registers[137] = 5900; // B Phase Demand Current Max TimeStamp
    registers[141] = 6000; // N Phase Demand Current Max TimeStamp
    registers[145] = 6100; // R Phase Voltage
    registers[147] = 6200; // Y Phase Voltage
    registers[149] = 6300; // B Phase Voltage
    registers[151] = 6400; // R - Y Phase - Phase Voltage
    registers[153] = 6500; // Y - B Phase - Phase Voltage
    registers[155] = 6600; // B - R Phase - Phase Voltage
    registers[157] = 6700; // Residual Voltage
    registers[159] = 6800; // R Phase Voltage Max
    registers[161] = 6900; // Y Phase Voltage Max
    registers[163] = 7000; // B Phase Voltage Max
    registers[165] = 7100; // R - Y Phase - Phase Voltage Max
    registers[167] = 7200; // Y - B Phase - Phase Voltage Max
    registers[169] = 7300; // B - R Phase - Phase Voltage Max
    registers[171] = 7400; // Residual Voltage Max
    registers[173] = 7500; // R Phase Voltage Max Time Stamp
    registers[177] = 7600; // Y Phase Voltage Max Time Stamp
    registers[181] = 7700; // B Phase Voltage Max Time Stamp
    registers[185] = 7800; // R - Y Phase Voltage Max Time Stamp
    registers[189] = 7900; // Y - B Phase Voltage Max Time Stamp
    registers[193] = 8000; // B - R Phase Voltage Max Time Stamp
    registers[197] = 8100; // Residual Voltage Max Time Stamp
    registers[201] = 8200; // R Phase Voltage Min
    registers[203] = 8300; // Y Phase Voltage Min
    registers[205] = 8400; // B Phase Voltage Min
    registers[207] = 8500; // R - Y Phase Voltage Min
    registers[209] = 8600; // Y - B Phase Voltage Min
    registers[211] = 8700; // B - R Phase Voltage Min
    registers[213] = 8800; // Residual Voltage Min
    registers[215] = 8900; // R Phase Voltage Min Time Stamp
    registers[219] = 9000; // Y Phase Voltage Min Time Stamp
    registers[223] = 9100; // B Phase Voltage Min Time Stamp
    registers[227] = 9200; // R - Y Phase Voltage Min Time Stamp
    registers[231] = 9300; // Y - B Phase Voltage Min Time Stamp
    registers[235] = 9400; // B - R Phase Voltage Min Time Stamp
    registers[239] = 9500; // Residual Voltage Min Time Stamp
    registers[243] = 9600; // Phase - N Average Voltage
    registers[245] = 9700; // Line - Line Average Voltage
}



// Modbus slave function to respond to read requests
void start_slave() {
    modbus_t *mb;       // Modbus context
    modbus_mapping_t *mb_mapping;  // Modbus mapping for registers
    int s;              // Socket for the slave server

    // Initialize the Modbus slave
    mb = modbus_new_tcp(NULL, MODBUS_PORT); // Listen on all IP addresses on port 502
    if (mb == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        exit(1);
    }

    // Initialize the register map
    mb_mapping = modbus_mapping_new(0, 0, 1000, 0);  // 1000 registers (300001-300100)
    if (mb_mapping == NULL) {
        fprintf(stderr, "Unable to allocate memory for the mapping\n");
        modbus_free(mb);
        exit(1);
    }

    // Set predefined values in the register map
    for (int i = 0; i < 1000; i++) {
        mb_mapping->tab_registers[i] = registers[i];
    }

    // Open the Modbus TCP server
    s = modbus_tcp_listen(mb, 1);
    if (s == -1) {
        fprintf(stderr, "Unable to open Modbus TCP server\n");
        modbus_mapping_free(mb_mapping);
        modbus_free(mb);
        exit(1);
    }

    // Accept incoming connections
    modbus_tcp_accept(mb, &s);
    printf("Slave started and listening...\n");

    while (1) {
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int rc;

        // Receive the request from the master
        rc = modbus_receive(mb, query);

	if (rc > 0) {
            // Process the request and send the response
	    //
	    
             /* 
	      *
	      * before sending repliy to the modbus i need do send the data the can bus 
	      *
	      * */

            modbus_reply(mb, query, rc, mb_mapping);
        }
    }

    // Cleanup
    modbus_mapping_free(mb_mapping);
    modbus_free(mb);
}



int main() {
    
    initialize_registers();
    start_slave();
    return 0;
}

