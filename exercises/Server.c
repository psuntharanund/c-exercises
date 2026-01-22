// Build: gcc -Wall -Wextra -O2 -o server server.c
// Run:   ./Server 6000

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

// Read a line (ending with '\n') from a socket into buf.
// Returns number of bytes placed in buf (excluding '\0'), 0 on clean EOF.
// Ensures buf is NUL-terminated.
static ssize_t recv_line(int sockfd, char *buf, size_t buflen) {
    if (buflen == 0){
        return -1;
    }
    size_t i = 0;
    while (i + 1 < buflen) {
        char c;
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n == 0) {               // peer closed connection
            if (i == 0) return 0;   // EOF and nothing read
            break;                  // EOF after some data
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

int main(int argc, char *argv[]) {
    int port = 6000;
    if (argc >= 2){
        port = atoi(argv[1]);
    }
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port.\n");
        return EXIT_FAILURE;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0){
        die("socket");
    }
    // Allow quick restart after Ctrl+C (avoid "Address already in use")
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        die("setsockopt");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET; //IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //listen on all network interfaces
    addr.sin_port        = htons((uint16_t)port); //use port which is converted to network byte order 

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        die("bind");
    }
    if (listen(listen_fd, 1) < 0){
        die("listen");
    }
    printf("Server has been started.\n");
    printf("Waiting to establish connection with client...\n");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0){
        die("accept");
    }
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    printf("Client connection established from %s:%u.\n",
           client_ip, (unsigned)ntohs(client_addr.sin_port));

    char line[1024];
    while (1) {
        ssize_t n = recv_line(client_fd, line, sizeof(line));
        if (n == 0) {
            printf("Client closed the connection.\n");
            break;
        }
        if (n < 0){
            die("recv");
        }
        // Strip trailing newline for nicer printing/comparison
        line[strcspn(line, "\r\n")] = '\0';

        printf("%s\n", line);
        fflush(stdout);

        if (strcmp(line, "stop") == 0 || strcmp(line, "STOP") == 0) {
            printf("Closing connection now.\n");
            break;
        }
    }

    close(client_fd);
    close(listen_fd);
    return 0;
}

