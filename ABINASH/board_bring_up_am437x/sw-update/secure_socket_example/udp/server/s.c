#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define SERVER_PORT 8080
#define BROADCAST_IP "192.168.0.255"
#define BUFFER_SIZE 1024
#define CERT_FILE "server.crt"
#define KEY_FILE "server.key"

SSL_CTX *create_context() {
    const SSL_METHOD *method = DTLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx) {
    if (SSL_CTX_use_certificate_file(ctx, CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        exit(EXIT_FAILURE);
    }
}

int main() {
    int server_fd;
    struct sockaddr_in broadcast_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int broadcastEnable = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("Failed to enable broadcast");
        exit(EXIT_FAILURE);
    }

    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, BROADCAST_IP, &broadcast_addr.sin_addr);

    while (1) {
        printf("Enter message to broadcast (type 'exit' to quit): ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (strcmp(buffer, "exit") == 0) break;

        // Create new SSL connection for each broadcast
        SSL *ssl = SSL_new(ctx);
        BIO *bio = BIO_new_dgram(server_fd, BIO_NOCLOSE);
        SSL_set_bio(ssl, bio, bio);
        
        // Set connected state for broadcast
        BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &broadcast_addr);
        SSL_set_connect_state(ssl);

        // Encrypt and send message
        int bytes_sent = SSL_write(ssl, buffer, strlen(buffer));
        if (bytes_sent <= 0) {
            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            continue;
        }
        printf("Encrypted message sent: %s\n", buffer);

        // Wait for secure ACK
        struct timeval timeout = {2, 0};
        setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        int recv_len = SSL_read(ssl, buffer, BUFFER_SIZE - 1);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("Received secure ACK from client: %s\n", buffer);
        } else {
            printf("ERROR: No acknowledgment received within 2 seconds.\n");
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    close(server_fd);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
