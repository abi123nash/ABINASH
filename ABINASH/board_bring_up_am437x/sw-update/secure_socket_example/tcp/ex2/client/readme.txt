sudo apt update
sudo apt install libssl-dev openssl

gcc c.c -o client -lssl -lcrypto

openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
