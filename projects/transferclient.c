#include <stdio.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#define BUFSIZE 512

#define USAGE                                                \
  "usage:\n"                                                 \
  "  transferclient [options]\n"                             \
  "options:\n"                                               \
  "  -p                  Port (Default: 61321)\n"            \
  "  -s                  Server (Default: localhost)\n"      \
  "  -h                  Show this help message\n"           \
  "  -o                  Output file (Default cs6200.txt)\n" 

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"output", required_argument, NULL, 'o'},
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
    int option_char = 0;
    char *hostname = "localhost";
    unsigned short portno = 61321;
    char *filename = "cs6200.txt";

    setbuf(stdout, NULL);

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "s:p:o:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 's': // server
            hostname = optarg;
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        case 'o': // filename
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        }
    }

    if (NULL == hostname) {
        fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
        exit(1);
    }

    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }

    /* Socket Code Here */
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%hu", portno);

    struct addrinfo filters, *serverResults, *serverResultsParser;
    memset(&filters, 0, sizeof(filters));
    filters.ai_family = AF_UNSPEC;
    filters.ai_socktype = SOCK_STREAM;

    int clientReturnCode = getaddrinfo(hostname, portstr, &filters, &serverResults);
    if (clientReturnCode != 0){
        return 1;
    }

    int sockfd = -1;
    for (serverResultsParser = serverResults; 
    serverResultsParser != NULL; 
    serverResultsParser = serverResultsParser->ai_next){
      sockfd = socket(serverResultsParser->ai_family,
                        serverResultsParser->ai_socktype,
                        serverResultsParser->ai_protocol);
        if (sockfd < 0){
            continue;
        }

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
    
    int inbound = open(filename, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    if (inbound < 0){
        close(sockfd);
        exit(1);
    }

    char msgBuffer[BUFSIZE];

    for (;;){
        ssize_t fileReceive = recv(sockfd, msgBuffer, sizeof(msgBuffer), 0);
        if (fileReceive == 0){
            break;
        }
        if (fileReceive < 0){
            close(sockfd);
            exit(1);
        }

        size_t fileULByte = 0;
        while (fileULByte < (size_t) fileReceive){
            ssize_t writeFile = write(inbound, msgBuffer + fileULByte, (size_t)fileReceive- fileULByte);
            if (writeFile <= 0){
                close(sockfd);
                exit(1);
            }
            fileULByte+=(size_t)writeFile;
        }
    }
}
