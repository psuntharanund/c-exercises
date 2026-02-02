#include "gfserver-student.h"

// Modify this file to implement the interface specified in
 // gfserver.h.
static int find_header_tail(const char *buf, size_t byte){
    for (size_t i = 0; i + 3 < byte; i++){
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n'){
            return (int)(i + 4);
        }
    }
    return -1;
}

struct gfcontext_t{
    int contCFD;
};

struct gfserver_t{
    unsigned short port;
    int max_npending;
    size_t file_len;
    size_t len;
    const void *data;
    const char *path;
    void* arg;
    gfh_error_t (*handler)(gfcontext_t **, const char *, void*);
};

void gfs_abort(gfcontext_t **ctx){
    if (ctx == NULL || *ctx == NULL){
        return;
    }

    if ((*ctx)->contCFD >= 0){
        shutdown((*ctx)->contCFD, SHUT_RDWR);
        close((*ctx)->contCFD);
        (*ctx)->contCFD = -1;
    }
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len){
    if (ctx == NULL || *ctx == NULL){
        return -1;
    }

    if (data == NULL && len > 0){
        return -1;
    }

    const char *msgBuffer = (const char *)data;
    size_t totalBytes = 0;

    while (totalBytes < len){
        ssize_t sent = send((*ctx)->contCFD, msgBuffer + totalBytes, len - totalBytes, 0);
        if (sent < 0 || sent == 0){
            return -1;
        }
        totalBytes+=(size_t)sent;
    }
    return (ssize_t)totalBytes;
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len){
    if (ctx == NULL || *ctx == NULL){
        return -1;
    }

    const char *fstatus = NULL;
    switch(status){
        case GF_OK:             fstatus = "OK"; break;
        case GF_FILE_NOT_FOUND: fstatus = "FILE_NOT_FOUND"; break;
        case GF_ERROR:          fstatus = "ERROR"; break;
        case GF_INVALID:        fstatus = "INVALID"; break;
        default:                fstatus = "INVALID"; status = GF_INVALID; break;
    }

    char header[256];
    int headerLen;
    if (status == GF_OK){
        headerLen = snprintf(header, sizeof(header), "GETFILE %s %zu\r\n\r\n", fstatus, file_len);
    }   else{
        headerLen = snprintf(header, sizeof(header), "GETFILE %s\r\n\r\n", fstatus);
    }

    if (headerLen < 0 || (size_t)headerLen >= sizeof(header)){
        return -1;
    }
    return gfs_send(ctx, header, (size_t)headerLen);
}   

gfserver_t* gfserver_create(){
    gfserver_t *request = calloc(1, sizeof(*request));
    if (request == 0){
        return NULL;
    }
    request->port = 0;
    request->file_len = 0;
    request->len = 0;
    request->max_npending = 0;

    return request;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port){
    if (gfs !=  NULL && *gfs != NULL){
        (*gfs)->port = port; 
    }
}
void gfserver_serve(gfserver_t **gfs){
    if (gfs == NULL ||  *gfs == NULL){
        return;
    }

    gfserver_t *serve = *gfs;

    /* Create socket for clients to connect to */
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%hu", serve->port);

    struct addrinfo filters, *serverResults, *serverResultsParser;
    memset(&filters, 0, sizeof(filters));
    filters.ai_family = AF_UNSPEC;
    filters.ai_socktype = SOCK_STREAM;
    filters.ai_flags = AI_PASSIVE;

    int returnServerCode = getaddrinfo(NULL, portstr, &filters, &serverResults);
    if (returnServerCode != 0){
        return;
    }

    int listenfd = -1;
    for (serverResultsParser = serverResults; serverResultsParser != NULL; serverResultsParser = serverResultsParser->ai_next){
        listenfd = socket(serverResultsParser->ai_family,
                          serverResultsParser->ai_socktype,
                          serverResultsParser->ai_protocol);
        if (listenfd < 0){
            continue;
        }

        int inUse = 1;
        (void)setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &inUse, sizeof(inUse));

        if (bind(listenfd, serverResultsParser->ai_addr, serverResultsParser->ai_addrlen) == 0){
            break;
        }

        close(listenfd);
        return;
    }
    freeaddrinfo(serverResults);

    if (listenfd < 0){
        close(listenfd);
        return;
    }

    if (listen(listenfd, serve->max_npending) != 0){
        close(listenfd);
        return;
    }
    
    /* Begin going through what you will receive from the client and send to the client */
    for (;;){
        int clientfd = accept(listenfd, NULL, NULL);
        if (clientfd < 0){
            continue;
        }
        gfcontext_t *ctx = calloc(1, sizeof(*ctx));
        if (ctx == NULL){
            close(clientfd);
            continue;
        }
        ctx->contCFD = clientfd;

        /* Read the request's header */
        char headerBuffer[1024];
        size_t parsed = 0;
        int headerEnd = -1;

        while (headerEnd < 0){
            if (parsed == sizeof(headerBuffer)){
                break;
            }
            ssize_t requestHeader = recv(clientfd, headerBuffer + parsed, sizeof(headerBuffer) - parsed, 0);
            if (requestHeader <= 0){
                break;
            }
            parsed+=(size_t)requestHeader;
            headerEnd = find_header_tail(headerBuffer, parsed);
        }

        /* If the header is malformed or closed early */
        if (headerEnd < 0){
            gfcontext_t *errorPTR = ctx;
            gfs_sendheader(&errorPTR, GF_INVALID, 0);
            gfs_abort(&errorPTR);
            free(ctx);
            continue;
        }

        char headerContent[1024];
        size_t headerLen = (size_t)headerEnd;

        if (headerLen >= sizeof(headerBuffer)){
            headerLen = sizeof(headerBuffer) - 1;
        }

        memcpy(headerContent, headerBuffer, headerLen);
        headerBuffer[headerLen] = '\0';

        char headerOne[16] = {0};
        char headerTwo[32] = {0};
        char path[1024] = {0};

        int headerParser = sscanf(headerBuffer, "%15s %31s %1023s", headerOne, headerTwo, path);

        int badHeaderCount = 0;
        if (headerParser != 3){
            badHeaderCount = 1;
        } else if (strcmp(headerOne, "GETFILE") != 0){
            badHeaderCount = 1;
        } else if (strcmp(headerTwo, "GET") != 0){
            badHeaderCount = 1;
        } else if (path[0] != '/'){
            badHeaderCount = 1;
        } 

        gfcontext_t *errorPTR = ctx;

        if (badHeaderCount != NULL){
            gfs_sendheader(&errorPTR, GF_INVALID, 0);
            gfs_abort(&errorPTR);
            free(ctx);
            continue;
        }
        
        if (serve->handler != NULL){
            gfh_error_t herr = serve->handler(&errorPTR, path, serve->arg);
            (void) herr;
        } else {
            gfs_sendheader(&errorPTR, GF_ERROR, 0);
        }
        
        if (ctx->contCFD >= 0){
            shutdown(ctx->contCFD, SHUT_RDWR);
            close(ctx->contCFD);
            ctx->contCFD = -1;
        }
        free(ctx);
    }   
}

void gfserver_set_handlerarg(gfserver_t **gfs, void* arg){
    if (gfs !=  NULL && *gfs != NULL){
        (*gfs)->arg = arg;
    }
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void*)){
    if (gfs !=  NULL && *gfs != NULL){
        (*gfs)->handler = handler; 
    }
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending){
    if (gfs !=  NULL && *gfs != NULL){
        (*gfs)->max_npending = max_npending;
    }
}

