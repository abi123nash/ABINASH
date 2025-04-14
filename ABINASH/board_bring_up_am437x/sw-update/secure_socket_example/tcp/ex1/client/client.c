// Secure Client Code (secure_client.c)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>

void debug_print(const char* func, int line) {
    printf("Zumi Fun: %s Line: %d\r\n", func, line);
}

// Macro to simplify usage of the debug_print function
#define p() debug_print(__func__, __LINE__)



#define PORT 12345
#define BUFFER_SIZE 1024

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSL_CTX *create_context() 
{
    const SSL_METHOD *method = TLS_client_method();
p();
    SSL_CTX *ctx = SSL_CTX_new(method);
p();
    if (!ctx) 
    {
p();
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void calculate_sha256(const char *file_path, unsigned char *hash) {
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("File open error");
        exit(EXIT_FAILURE);
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        SHA256_Update(&sha256, buffer, bytesRead);
    }
    SHA256_Final(hash, &sha256);
    fclose(file);
}

int main() 
{
p();
    int sock;
p();
    struct sockaddr_in server_address;

p();
    init_openssl();
p();
    SSL_CTX *ctx = create_context();
p();

    sock = socket(AF_INET, SOCK_STREAM, 0);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = inet_addr("192.168.0.153");

p();
    connect(sock, (struct sockaddr *)&server_address, sizeof(server_address));
p();

    SSL *ssl = SSL_new(ctx);
p();
    SSL_set_fd(ssl, sock);
p();

    if (SSL_connect(ssl) <= 0) 
    {
p();
        ERR_print_errors_fp(stderr);
    } 
    else 
    {
p();
        FILE *file = fopen("received_file.txt", "wb");
p();
        unsigned char buffer[BUFFER_SIZE];
        size_t bytesRead;

p();
        while ((bytesRead = SSL_read(ssl, buffer, BUFFER_SIZE)) > SHA256_DIGEST_LENGTH) 
	{
            fwrite(buffer, 1, bytesRead, file);
        }

        fclose(file);

        unsigned char received_hash[SHA256_DIGEST_LENGTH];
        memcpy(received_hash, buffer + (bytesRead - SHA256_DIGEST_LENGTH), SHA256_DIGEST_LENGTH);

        unsigned char calculated_hash[SHA256_DIGEST_LENGTH];
        calculate_sha256("received_file.txt", calculated_hash);

        SSL_write(ssl, (memcmp(received_hash, calculated_hash, SHA256_DIGEST_LENGTH) == 0) ? "ACK" : "NACK", 4);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}

