CC=arm-linux-gnueabihf-gcc
CFLAGS=-I/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib/include -Wall
LDFLAGS=-L/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib/lib -lmodbus
TARGET=modbus_can
SRC=modbus_can.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

