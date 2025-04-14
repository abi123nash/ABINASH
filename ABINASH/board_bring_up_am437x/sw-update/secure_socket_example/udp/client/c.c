#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define CLIENT_PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_CERT "server.crt"

SSL_CTX *create_context() {
    const SSL_METHOD *method = DTLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    return ctx;
}

void configure_context(SSL_CTX *ctx) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    if (!SSL_CTX_load_verify_locations(ctx, SERVER_CERT, NULL)) {
        fprintf(stderr, "Error loading trust store\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

int main() {
    int client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX *ctx = create_context();
    configure_context(ctx);

    if ((client_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(CLIENT_PORT);

    if (bind(client_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        socklen_t addr_len = sizeof(client_addr);
        
        // Create new SSL connection for each received message
        SSL *ssl = SSL_new(ctx);
        BIO *bio = BIO_new_dgram(client_fd, BIO_NOCLOSE);
        SSL_set_bio(ssl, bio, bio);
        SSL_set_accept_state(ssl);

        // Receive encrypted message
        int recv_len = SSL_read(ssl, buffer, BUFFER_SIZE - 1);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            printf("Received encrypted message from server: %s\n", buffer);

            // Get peer address for response
            BIO_ctrl(bio, BIO_CTRL_DGRAM_GET_PEER, 0, &client_addr);

            // Send secure acknowledgment
            const char *ack_msg = "ACK";
            SSL_write(ssl, ack_msg, strlen(ack_msg));
            printf("Sent secure acknowledgment to server.\n");
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    close(client_fd);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    return 0;
}
