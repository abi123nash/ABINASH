#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 4096
#define CERT_FILE "server.crt"
#define KEY_FILE "server.key"
#define SHUTDOWN_TIMEOUT 5  // seconds to wait for clean shutdown

volatile sig_atomic_t running = 1;
SSL_CTX *ctx = NULL;  // Make global for cleanup

void handle_signal(int sig) {
    if (running) {
        printf("\nReceived signal %d, shutting down gracefully...\n", sig);
        running = 0;
    } else {
        printf("\nForce shutdown...\n");
        exit(EXIT_FAILURE);
    }
}

void init_openssl() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() {
    EVP_cleanup();
}

SSL_CTX *create_context() {
    const SSL_METHOD *method = TLS_server_method();
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

    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
    SSL_CTX_set_ecdh_auto(ctx, 1);
}

void handle_client(SSL *ssl, int client_fd) {
    char buffer[BUFFER_SIZE];
    int bytes_received;

    do {
        bytes_received = SSL_read(ssl, buffer, sizeof(buffer));
        if (bytes_received > 0) {
            printf("Received: %.*s\n", bytes_received, buffer);
            
            if (SSL_write(ssl, buffer, bytes_received) <= 0) {
                perror("SSL write failed");
                break;
            }
        } else if (bytes_received == 0) {
            printf("Client disconnected\n");
        } else {
            int err = SSL_get_error(ssl, bytes_received);
            if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                continue;
            }
            perror("SSL read error");
            break;
        }
    } while (bytes_received > 0 && running);

    // Proper SSL shutdown sequence
    int shutdown_ret = SSL_shutdown(ssl);
    if (shutdown_ret == 0) {
        // Call again for bidirectional shutdown
        SSL_shutdown(ssl);
    }
    
    SSL_free(ssl);
    close(client_fd);
}

void cleanup_resources(int server_fd) {
    printf("Cleaning up resources...\n");
    
    if (server_fd != -1) {
        shutdown(server_fd, SHUT_RDWR);
        close(server_fd);
    }
    
    if (ctx != NULL) {
        SSL_CTX_free(ctx);
    }
    
    cleanup_openssl();
}

void wait_for_children() {
    int status;
    pid_t pid;
    time_t start_time = time(NULL);
    
    printf("Waiting for child processes to finish...\n");
    while ((pid = waitpid(-1, &status, WNOHANG))) {
        if (pid == -1) {
            if (errno == ECHILD) {
                break;  // No more children
            }
            perror("waitpid error");
            break;
        } else if (pid == 0) {
            if (time(NULL) - start_time > SHUTDOWN_TIMEOUT) {
                printf("Timeout reached, forcing shutdown\n");
                break;
            }
            sleep(1);  // Wait a bit before checking again
            continue;
        }
        
        if (WIFEXITED(status)) {
            printf("Child %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child %d killed by signal %d\n", pid, WTERMSIG(status));
        }
    }
}

int main() {
    int server_fd = -1, client_fd = -1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pid_t child_pid;

    // Signal handling for graceful shutdown
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGINT, &sa, NULL) == -1 || 
        sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    // Initialize OpenSSL
    init_openssl();
    ctx = create_context();
    configure_context(ctx);

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket creation failed");
        cleanup_resources(-1);
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        cleanup_resources(server_fd);
        exit(EXIT_FAILURE);
    }

    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind failed");
        cleanup_resources(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen failed");
        cleanup_resources(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (running) {
        // Accept new connection with timeout
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ready = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal
            }
            perror("select failed");
            break;
        } else if (ready == 0) {
            continue;  // Timeout, check running flag again
        }

        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept failed");
            continue;
        }

        printf("Connection from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Create SSL connection
        SSL *ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client_fd);

        if (SSL_accept(ssl) <= 0) {
            ERR_print_errors_fp(stderr);
            close(client_fd);
            SSL_free(ssl);
            continue;
        }

        // Fork to handle client
        child_pid = fork();
        if (child_pid == -1) {
            perror("fork failed");
            close(client_fd);
            SSL_free(ssl);
            continue;
        }

        if (child_pid == 0) { // Child process
            close(server_fd);
            handle_client(ssl, client_fd);
            exit(EXIT_SUCCESS);
        } else { // Parent process
            close(client_fd);
            SSL_free(ssl);
        }
    }

    // Graceful shutdown
    printf("Initiating graceful shutdown...\n");
    cleanup_resources(server_fd);
    wait_for_children();
    
    printf("Server shutdown complete\n");
    return EXIT_SUCCESS;
}
