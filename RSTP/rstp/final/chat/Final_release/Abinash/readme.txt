arm-linux-gnueabihf-gcc -static client.c -o client_imx -lpthread

arm-linux-gnueabihf-gcc -static client_AM437x.c -o client_AM437x -lpthread


gcc -static client.c -o client_pc -lpthread

gcc -static server_pc.c -o server_pc 

