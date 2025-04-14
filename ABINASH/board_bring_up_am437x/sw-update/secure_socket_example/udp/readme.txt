gcc s.c -o udp_server -lssl -lcrypto
gcc c.c -o udp_client -lssl -lcrypto



openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
