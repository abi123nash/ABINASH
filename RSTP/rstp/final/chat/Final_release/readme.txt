arm-linux-gnueabihf-gcc -static client_imx.c -o client -lpthread

arm-linux-gnueabihf-gcc -static client_AM437x.c -o client -lpthread


gcc -static c_pc.c -o client -lpthread

gcc -static server.c -o server -lpthread

