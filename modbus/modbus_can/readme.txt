
git clone https://github.com/stephane/libmodbus.git
cd libmodbus
./autogen.sh



sudo apt-get update
sudo apt-get install libtool automake autoconf m4
sudo apt-get update
sudo apt-get install g++-arm-linux-gnueabihf


./configure --host=arm-linux-gnueabihf --prefix=/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib --disable-tests

autoreconf -i

make -j$(nproc)

sudo make install





static
======

 ./configure --host=arm-linux-gnueabihf --prefix=/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib  --disable-tests --enable-static  --disable-shared


./configure --host=arm-linux-gnueabihf --prefix=/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib  --disable-tests --enable-static --disable-shared CFLAGS="-static" LDFLAGS="-static -lm"

make -j$(nproc)
make install










single line command
-------------------

arm-linux-gnueabihf-gcc modbus_can.c -o modbus_can -L/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib/lib -I/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib/include -lmodbus


static
--------

arm-linux-gnueabihf-gcc -o  modbus_can modbus_can.c  -I/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib/include -L/home/zumi/Abinash/am4376_board_bringup_new_project/modbus/modbus_can/lib/lib -static -lmodbus -lm


gcc -o modbus_can modbus_can.c -lmodbus




sudo apt-get install libmodbus-dev

gcc -o master master.c -lmodbus
