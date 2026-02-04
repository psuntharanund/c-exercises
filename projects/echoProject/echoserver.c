#include <unistd.h>
#include <sys/socket.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#define BUFSIZE 1024

#define USAGE                                                        \
    "usage:\n"                                                         \
    "  echoserver [options]\n"                                         \
    "options:\n"                                                       \
    "  -p                  Port (Default: 14757)\n"                    \
    "  -m                  Maximum pending connections (default: 5)\n" \
    "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"maxnpending",   required_argument,      NULL,           'm'},
    {"port",          required_argument,      NULL,           'p'},
    {"help",          no_argument,            NULL,           'h'},
    {NULL,            0,                      NULL,             0}
};

int main(int argc, char **argv) {
    int option_char;
    int portno = 14757; /* port to listen on */
    int maxnpending = 5;
  
    // Parse and set command line arguments
    while ((option_char = getopt_long(argc, argv, "p:m:hx", gLongOptions, NULL)) != -1) {
        switch (option_char) {
        case 'm': // server
            maxnpending = atoi(optarg);
            break; 
        case 'h': // help
            fprintf(stdout, "%s ", USAGE);
            exit(0);
            break;
        case 'p': // listen-port
            portno = atoi(optarg);
            break;                                        
        default:
            fprintf(stderr, "%s ", USAGE);exit(1);
        }
    }

    setbuf(stdout, NULL); // disable buffering

    if ((portno < 1025) || (portno > 65535)) {
        fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__, portno);
        exit(1);
    }
    if (maxnpending < 1) {
        fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__, maxnpending);
        exit(1);
    }

    /* Socket Code Here */

    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%hu", portno);
            
    /* We need to make it available for both IPv4 and IPv6, and we can do that via getaddrinfo */
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

    if (listen(listenfd, maxnpending) != 0){
        close(listenfd);
        exit(1);
    }


    for (;;){
        int clientfd = accept(listenfd, NULL, NULL);
        if (clientfd < 0){
            close(listenfd);
            exit(1);
        }

        for(;;){
            char msgBuffer[16];
            ssize_t msg = recv(clientfd, msgBuffer, sizeof(msgBuffer), 0);

            if (msg == 0){
                break;
            }
            if (msg < 0 || msg > 15){
                close(clientfd);
                close(listenfd);
                exit(1);
            }

            ssize_t sent = send(clientfd, msgBuffer, (size_t)msg, 0);
            if (sent != msg){
                close(clientfd);
                close(listenfd);
                exit(1);
            }
        }
    close(clientfd);
    } 
}
