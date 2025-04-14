vi master.c
sudo apt update
sudo apt install libmodbus-dev
gcc -o modbus_master master.c -lmodbus
