#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>

#define BUFSIZE 512

#define USAGE                                                \
    "usage:\n"                                               \
    "  transferserver [options]\n"                           \
    "options:\n"                                             \
    "  -f                  Filename (Default: 6200.txt)\n"   \
    "  -p                  Port (Default: 61321)\n"          \
    "  -h                  Show this help message\n"         \

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"port", required_argument, NULL, 'p'},
    {"filename", required_argument, NULL, 'f'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
    int option_char;
    int portno = 61321;             /* port to listen on */
    char *filename = "6200.txt"; /* file to transfer */

    setbuf(stdout, NULL); // disable buffering

    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:hf:x", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'p': // listen-port
            portno = atoi(optarg);
            break;
        case 'f': // file to transfer
            filename = optarg;
            break;
        case 'h': // help
            fprintf(stdout, "%s", USAGE);
            exit(0);
            break;
        default:
            fprintf(stderr, "%s", USAGE);
            exit(1);
        }
    }


    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    
    if (NULL == filename) {
        fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
        exit(1);
    }

    /* Socket Code Here */
    
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%hu", portno);

    struct addrinfo filters, *serverResults, *serverResultsParser;
    memset(&filters, 0, sizeof(filters));
    filters.ai_family = AF_UNSPEC;
    filters.ai_socktype = SOCK_STREAM;
    filters.ai_flags = AI_PASSIVE;

    int returnCode = getaddrinfo(NULL, portstr, &filters, &serverResults);
    if (returnCode != 0){
        exit(1);
    }

    int listenfd = -1;
    for (serverResultsParser = serverResults; serverResultsParser != NULL; serverResultsParser = serverResultsParser->ai_next){
                                                                                                      
        listenfd = socket(serverResultsParser->ai_family,
                          serverResultsParser->ai_socktype, 
                          serverResultsParser->ai_protocol);

        if (listenfd < 0){
            continue;
        }
        
        /* If address is already in use, this will avoid it on restart */
        int inUse = 1;
        (void)setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &inUse, sizeof(inUse));

        if (bind(listenfd, serverResultsParser->ai_addr, serverResultsParser->ai_addrlen) == 0){
            break;
        }
        
        close(listenfd);
        listenfd = -1;
    }   

    freeaddrinfo(serverResults);

    if (listenfd < 0){
        exit(1);
    }
    
    if (listen(listenfd, 5) != 0){
        close(listenfd);
        exit(1);
    }

    for (;;){
        int clientfd = accept(listenfd, (struct sockaddr *)NULL, (socklen_t *)NULL);
        if (clientfd < 0){
            close(listenfd);
            exit(1);
        }

        FILE *outbound = fopen(filename, "rb");
        if (!outbound){
            close(listenfd);
            return 1;
        }


        for(;;){
            char msgBuffer[BUFSIZE];
            size_t fileDL = fread(msgBuffer, 1, sizeof(msgBuffer), outbound);
            if (fileDL == 0){
                break;
            }
            
            size_t fileDLByte = 0;
            while (fileDLByte < fileDL){
                ssize_t sent = send(clientfd, msgBuffer + fileDLByte, fileDL - fileDLByte, 0);
                if (sent <= 0){
                    break;
                }
                fileDLByte+=(size_t)sent;
            }
        }
        fclose(outbound);
        shutdown(clientfd, SHUT_WR);
        close(clientfd);
    }
}
