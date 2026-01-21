#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdio.h>

/* Be prepared accept a response of this length */
#define BUFSIZE 1024

#define USAGE                                                                       \
    "usage:\n"                                                                      \
    "  echoclient [options]\n"                                                      \
    "options:\n"                                                                    \
    "  -p                  Port (Default: 14757)\n"                                  \
    "  -s                  Server (Default: localhost)\n"                           \
    "  -m                  Message to send to server (Default: \"Hello Spring!!\")\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"message", required_argument, NULL, 'm'},
    {"port", required_argument, NULL, 'p'},
    {"server", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    unsigned short portno = 14757;
    char *hostname = "localhost";
    char *message = "Hello Spring!!";

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:m:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);exit(1);
        case 'm': // message
            message = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    if (NULL == message) {
        fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    
    /* This is used to locate the address the socket will be using */
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%hu", portno);
    
    // Since you want to make it available for both IPv4 and IPv6, you should use hints instead of server_addr as getaddrinfo suppo    rts both IPv4 and IPv6 as getaddrinfo doesn't write the results 
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    int returnCode = getaddrinfo(hostname, portstr, &hints, &res);
    if (returnCode != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_sterror(returnCode));
        return 1;
    }

    int sockfd = -1;
    for (p = res; p != NULL; p = p->ai_next){sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);

        if (sockfd < 0){
            printf("Unable to create socket. . . quitting application.\n");
            continue;

         /* This is to check if it can connect to the socket */
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0){
            break;
        }
        close(sockfd);
        sockfd = -1;
    }

    printf("Connected successfully to %s:%hu\n", hostname, portno);

    /* This will read the current line's input in the client server that the user types, up to 16 chars. */
    char line[16];
    while (1){
        // This reads one line from STDIN
        if (!fgets(line, sizeof(line), stdin)){
            break;
        }

        size_t linelength = strlen(line);
        // This checks if the client sent anyhting, if not it will keep asking
        if (linelength == 1 && line[0] == '\n'){
            continue;
        }

        if (line[linelength - 1] != '\n'){
            // If the line doesn't end with a newline char and there's room, we append by 1
            if (linelength + 1 < sizeof(line)){
                line[linelength] = '\n';
                line[linelength + 1] = '\0';
                linelength++;
            }
        }
        // This will send the message that is being asked of it
        size_t totalbytes = 0;
        while (totalbytes < linelength){  
            ssize_t sent = send(sockfd, line + totalbytes, linelength - totalbytes, 0);
            if (sent < 0){
                printf("You have put an invalid amount. Please try again.");
                break;
            }
            totalbytes += (size_t)sent;
        }}
    close(sockfd);
    return 0;
}
}
