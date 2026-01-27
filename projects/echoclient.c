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
    
    // Since you want to make it available for both IPv4 and IPv6, you should use filters instead of server_addr as getaddrinfo supports both IPv4 and IPv6 as getaddrinfo doesn't write the results 
    struct addrinfo filters, *serverResults, *serverResultsParser;
    memset(&filters, 0, sizeof(filters));
    filters.ai_family = AF_UNSPEC;
    filters.ai_socktype = SOCK_STREAM;
    
    int returnCode = getaddrinfo(hostname, portstr, &filters, &serverResults);
    if (returnCode != 0){
        return 1;
    }

    int sockfd = -1;
    for (serverResultsParser = serverResults; serverResultsParser != NULL; serverResultsParser = serverResultsParser->ai_next){
        sockfd = socket(serverResultsParser->ai_family, 
                        serverResultsParser->ai_socktype, 
                        serverResultsParser->ai_protocol);

        if (sockfd < 0){
            continue;
        }
         /* This is to check if it can connect to the socket */
        if (connect(sockfd, serverResultsParser->ai_addr, serverResultsParser->ai_addrlen) == 0){
            break;
        }
        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(serverResults);
    
    if (sockfd < 0){
        exit(1); 
    }

    /* This will read the current line's input in the client server that the user types, up to 16 chars. */
    char msgBuffer[16];
    size_t msgLen = strlen(message);
    if (msgLen > 15){
        exit(1);
    }

    ssize_t sent = send(sockfd, message, msgLen, 0);
    if (sent != (ssize_t)msgLen){
        exit(1);
    }

    ssize_t received = recv(sockfd, msgBuffer, sizeof(msgBuffer), 0);
    if (received <= 0 || received > 15){
        exit(1);
    }

    ssize_t written = write(STDOUT_FILENO, msgBuffer, (size_t)received);
    if (written != received){
        exit(1);
    }

    close(sockfd);
    return 0;
}
