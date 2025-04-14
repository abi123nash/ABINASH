# Compiler and flags
CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -g
#INCLUDES = -I/path/to/arm/sysroot/usr/include/modbus
LDFLAGS = -L/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/new/modbus_can/zlib/lib -lmodbus  
LDFLAGS = -lmodbus  



# Target name
TARGET = can_send_dynamic

# Source files
SRCS = modbus_can.c

# Object files
OBJS = $(SRCS:.c=.o)

# Build target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS)  -o $(TARGET) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean object files and target
clean:
	rm -f $(OBJS) $(TARGET)

