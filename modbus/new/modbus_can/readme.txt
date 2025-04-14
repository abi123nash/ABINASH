git clone https://github.com/stephane/libmodbus.git
cd libmodbus
./autogen.sh
sudo apt-get update
sudo apt-get install libtool automake autoconf m4
./configure --host=arm-linux-gnueabihf
autoreconf -i
make
sudo make install

