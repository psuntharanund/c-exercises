// Build: gcc -Wall -Wextra -O2 -o client client.c
// Run:   ./client 127.0.0.1 6000

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Send all bytes in buf (handles partial sends)
static void send_all(int sockfd, const char *buf, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(sockfd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            die("send");
        }
        sent += (size_t)n;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *address = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port.\n");
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) die("socket");

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, address, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "Invalid address: %s\n", address);
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        die("connect");
    }

    printf("Connected successfully to %s:%d\n", address, port);

    char line[1024];
    while (1) {
        // Read one line from stdin
        if (!fgets(line, sizeof(line), stdin)) {
            // EOF on stdin (Ctrl+D) -> stop
            break;
        }

        // Ensure it ends with '\n' (fgets usually keeps it, but if line too long it may not)
        size_t len = strlen(line);
        if (len == 0) continue;
        if (line[len - 1] != '\n') {
            // line was longer than buffer; append newline to keep protocol consistent
            if (len + 1 < sizeof(line)) {
                line[len] = '\n';
                line[len + 1] = '\0';
                len++;
            }
        }

        send_all(sockfd, line, len);

        // Compare without newline
        char tmp[1024];
        strncpy(tmp, line, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';
        tmp[strcspn(tmp, "\r\n")] = '\0';

        if (strcmp(tmp, "stop") == 0 || strcmp(tmp, "STOP") == 0){
            break;
        }
    }

    close(sockfd);
    return 0;
}

