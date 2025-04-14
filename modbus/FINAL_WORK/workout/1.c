#define CAN_INTERFACE "can0"

typedef struct __attribute__((packed)) {
    uint16_t data1;
    uint16_t data2;
    uint16_t data3;
    uint16_t data4;
    uint16_t data5;
    uint16_t data6;
    uint16_t data7;
    uint16_t data8;
    uint16_t data9;
    uint16_t data10;
    uint16_t data11;
    uint16_t data12;
    uint16_t data13;
    uint16_t data14;
    uint16_t data15;
    uint16_t data16;
    uint16_t data17;
    uint16_t data18;
    uint16_t data19;
    uint16_t data20;
    uint16_t data21;
    uint16_t data22;
    uint16_t data23;
    uint32_t data24;
    uint32_t data25;
    uint16_t data26;
} BR_DATA;

uint16_t calculate_crc(uint8_t *data, int length);
void send_can_request(int socket_fd);
int receive_can_response(int socket_fd, BR_DATA *br_data);

void can_send_receive_reassemble_fragment_data(int socket_fd) {
    BR_DATA br_data;
    memset(&br_data, 0, sizeof(BR_DATA));

    send_can_request(socket_fd);
    receive_can_response(socket_fd, &br_data);
}

void send_can_request(int socket_fd) {
    struct can_frame request_frame;
    request_frame.can_id = 0x01200333;
    request_frame.can_dlc = 0;
    
    if (write(socket_fd, &request_frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
        perror("CAN request send failed");
    }
}

int receive_can_response(int socket_fd, BR_DATA *br_data) {
    struct can_frame frame;
    uint8_t received_data[56] = {0};
    int received_bytes = 0;
    uint8_t frame_index;
    int expected_frames = 12;

    for (int i = 0; i < expected_frames; i++) {
        if (read(socket_fd, &frame, sizeof(struct can_frame)) < 0) {
            perror("CAN response receive failed");
            return -1;
        }

        frame_index = frame.data[0];
        uint16_t crc_received = (frame.data[6] << 8) | frame.data[7];

        if (calculate_crc(frame.data, 6) != crc_received) {
            printf("CRC Error on frame %d\n", frame_index);
            return -1;
        }

        memcpy(&received_data[frame_index * 5], &frame.data[1], 5);

        struct can_frame ack_frame;
        ack_frame.can_id = 0x01290333 + (frame_index * 5);
        ack_frame.can_dlc = 8;
        memset(ack_frame.data, 0, 8);
        ack_frame.data[0] = frame_index;
        ack_frame.data[6] = 0xFF;
        ack_frame.data[7] = 0xFE - frame_index;

        if (write(socket_fd, &ack_frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
            perror("CAN ACK send failed");
        }
    }

    memcpy(br_data, received_data, sizeof(BR_DATA));
    return 0;
}

uint16_t calculate_crc(uint8_t *data, int length) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
