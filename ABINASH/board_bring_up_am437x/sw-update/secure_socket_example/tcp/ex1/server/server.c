// Secure Server Code (secure_server.c)
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

void init_openssl() 
{
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

SSL_CTX *create_context() 
{
    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx) 
{
    if (SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) 
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

void calculate_sha256(const char *file_path, unsigned char *hash) 
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    FILE *file = fopen(file_path, "rb");
    
    if (!file) 
    {
        perror("File open error");
        exit(EXIT_FAILURE);
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) 
    {
        SHA256_Update(&sha256, buffer, bytesRead);
    }
    SHA256_Final(hash, &sha256);
    fclose(file);
}

void send_file(SSL *ssl, const char *file_path) 
{
    FILE *file = fopen(file_path, "rb");
    if (!file) 
    {
        perror("File open error");
        exit(EXIT_FAILURE);
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytesRead;
    while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) 
    {
        SSL_write(ssl, buffer, bytesRead);
    }
    fclose(file);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    calculate_sha256(file_path, hash);
    SSL_write(ssl, hash, SHA256_DIGEST_LENGTH);
}

int main() 
{
    int server_fd, client_socket;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);

    init_openssl();
    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);
    printf("Secure server listening on port %d\n", PORT);

    client_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
    if (client_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client_socket);
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
    } else {
        send_file(ssl, "file_to_send.txt");

        char ack[4];
        SSL_read(ssl, ack, sizeof(ack));
        printf("Client ACK: %s\n", ack);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
    close(server_fd);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    return 0;
}

