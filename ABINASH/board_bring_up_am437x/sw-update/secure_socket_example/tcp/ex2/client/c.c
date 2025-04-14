#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define SERVER_IP "192.168.0.153"
#define PORT 8080
#define BUFFER_SIZE 4096

void debug_print(const char* func, int line) {
    printf("Zumi Fun: %s Line: %d\r\n", func, line);
}

// Macro to simplify usage of the debug_print function
#define p() debug_print(__func__, __LINE__)




void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX *create_context() {
    const SSL_METHOD *method = TLS_client_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) {
        perror("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    // Verify server certificate
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    if (!SSL_CTX_load_verify_locations(ctx, "server.crt", NULL)) 
    {
        fprintf(stderr, "Error loading trust store\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    
    return ctx;
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr;
    SSL_CTX *ctx;
    SSL *ssl;

    // Initialize OpenSSL
    init_openssl();
    ctx = create_context();

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
        perror("connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Create SSL connection
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);

    if (SSL_connect(ssl) <= 0) 
    {
        ERR_print_errors_fp(stderr);
        close(sockfd);
        SSL_free(ssl);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server. Type 'quit' to exit.\n");

    char buffer[BUFFER_SIZE];
    while (1) {
        printf("Enter message: ");
        if (!fgets(buffer, sizeof(buffer), stdin)) {
            if (feof(stdin)) {
                printf("\nEOF received. Disconnecting.\n");
                break;
            }
            perror("fgets failed");
            continue;
        }

        // Remove newline
        buffer[strcspn(buffer, "\n")] = '\0';

        // Check for quit command
        if (strcmp(buffer, "quit") == 0) {
            break;
        }

        // Send message
        if (SSL_write(ssl, buffer, strlen(buffer)) <= 0) {
            perror("SSL write failed");
            break;
        }

        // Receive response
        int bytes_received = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Server response: %s\n", buffer);
        } else if (bytes_received == 0) {
            printf("Server disconnected\n");
            break;
        } else {
            int err = SSL_get_error(ssl, bytes_received);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue; // Retry
            }
            perror("SSL read error");
            break;
        }
    }

    // Cleanup
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    cleanup_openssl();

    return 0;
}
