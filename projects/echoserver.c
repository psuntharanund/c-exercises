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
    struct addrinfo hints, *results, *resultparser;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int returnCode = getaddrinfo(hostname, portstr, &hints, &results);
    if (returnCode != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_sterror(returnCode));
        return 1;
    }

    int listenfd = -1;
    for (resultparser = results; resultparser != NULL; resultparser = resultparser->ai_next){
                                                                                                      
        listenfd = socket(resultparser->ai_family, resultparser->ai_socktype, resultparser->ai_protocol);

        if (listenfd < 0){
            printf("Unable to create socket . . . quitting application.\n");
            continue;
        }
        
        if (bind(listenfd, resultparser->ai_addr, resultparser->ai_addrlen) == 0){
            break;
        }
        
        close(listenfd);
        listenfd = -1;
    }   

    printf("Connected successfully to %s:%hu\n", hostname, portno);

  }
}
