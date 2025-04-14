gcc server.c -o server -lssl -lcrypto
gcc client.c -o client -lssl -lcrypto


### Generate a Private Key

openssl genrsa -out server.key 2048
-----------------------------------

- **openssl genrsa** → Generate a private RSA key.
- **-out server.key** → Save the key to `server.key`.
- **2048** → The key length (2048 bits) for secure encryption.



### Create a Certificate Signing Request (CSR)

openssl req -new -key server.key -out server.csr
------------------------------------------------

- **openssl req** → Generate a certificate signing request.
- **-new** → Create a new CSR.
- **-key server.key** → Use the private key (`server.key`) for the CSR.
- **-out server.csr** → Save the CSR to `server.csr`.

You'll be asked for details like:
- **Country Name (C)** → 2-letter country code (e.g., `US` for the United States)
- **State (ST)** → Your state or province
- **Locality (L)** → City or locality
- **Organization (O)** → Your organization name
- **Common Name (CN)** → Domain name or IP (use `localhost` for local testing)


### Generate a Self-Signed Certificate

openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt
---------------------------------------------------------------------------------

- **openssl x509** → Generate an X.509 certificate (standard SSL certificate format).
- **-req** → Use the CSR to generate the certificate.
- **-days 365** → Certificate is valid for 365 days.
- **-in server.csr** → Input CSR (`server.csr`).
- **-signkey server.key** → Sign the certificate with your private key.
- **-out server.crt** → Save the certificate to `server.crt`.


### View the Certificate Details

openssl x509 -in server.crt -text -noout
-------------------------------------------

- **openssl x509** → Read the certificate.
- **-in server.crt** → Specify the certificate to read.
- **-text** → Display detailed certificate information.
- **-noout** → Suppress showing the encoded certificate.



openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
