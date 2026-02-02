#include "gfclient-student.h"

 // Modify this file to implement the interface specified in
 // gfclient.h.

// Since gfcrequest_t is an opaque pointer type in our gfclient.h file, we need to define what's inside it so that we can use it for all our other functions.
static int find_header_tail(const char *buf, size_t byte){
    for (size_t i = 0; i + 3 < byte; i++){
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n'){
            return (int)(i + 4);
        }
    }
    return -1;
}

struct gfcrequest_t{
    const char *path;
    const char *server;
    unsigned short port;
    gfstatus_t status;
    size_t bytes_received;
    size_t file_len;
    void (*writefunc)(void *data, size_t len, void *arg);
    void *writearg;
    void (*headerfunc)(void *data, size_t len, void *arg);
    void *headerarg;

};

// optional function for cleanup processing.
void gfc_cleanup(gfcrequest_t **gfr) {
    if (gfr != NULL && *gfr != NULL){
        free (*gfr);
        *gfr = NULL;
    }
}

gfcrequest_t *gfc_create() {
    gfcrequest_t *request = calloc(1, sizeof(*request));
    if (request == 0){
        return NULL;
    }
    request->port = 0;
    request->status = 0;
    request->bytes_received = 0;
    request->file_len = 0;
   
    return request;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr) {
    if (gfr == NULL || *gfr == NULL){
        return 0;
    }
    /* -> has higher precendence than () */
    return (*gfr)->bytes_received;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr) {
    if (gfr == NULL || *gfr == NULL){
        return GF_INVALID;
    }
    
    return (*gfr)->status;
}

size_t gfc_get_filelen(gfcrequest_t **gfr) {
    if (gfr == NULL || *gfr == NULL){
        return 0;
    }
    
    return (*gfr)->file_len;
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_perform(gfcrequest_t **gfr) {
    if (gfr == NULL || *gfr == NULL){
        return -1;    
    }

    gfcrequest_t *request = *gfr;

    if (request->server == NULL || request->path == NULL || request->port == 0){
        request->status = GF_INVALID;
        return -1;
    }
    
    request->bytes_received = 0;
    request->file_len = 0;
    request->status = GF_INVALID;
    
    /* Establish connection to server */
    char portstr[6];
    snprintf(portstr, sizeof(portstr), "%hu", request->port);

    struct addrinfo filters, *serverResults = NULL, *serverResultsParser = NULL;
    memset(&filters, 0, sizeof(filters));
    filters.ai_family = AF_UNSPEC;
    filters.ai_socktype = SOCK_STREAM;

    int clientReturnCode = getaddrinfo(request->server, portstr, &filters, &serverResults);
    if (clientReturnCode != 0){
        request->status = GF_INVALID;
        return -1;
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
        request->status = GF_INVALID;
        return -1;
    }
    
    /* Send line requesting for header */
    char reqBuffer[1024];
    int reqLen = snprintf(reqBuffer, sizeof(reqBuffer), "GETFILE GET %s\r\n\r\n", request->path);
    size_t totalBytes = 0;
    
    if (reqLen < 0 || (size_t)reqLen >= sizeof(reqBuffer)){
        request->status = GF_INVALID;
        close(sockfd);
        return -1;
    }

    while (totalBytes < (size_t)reqLen){
            ssize_t sent = send(sockfd, reqBuffer + totalBytes, (size_t)reqLen - totalBytes, 0);
            
            if (sent <= 0){
                request->status = GF_ERROR;
                close(sockfd);
                return -1;
            }
        totalBytes+=(size_t)sent;
    }
    
    /* Receive the header until the \r\n\r\n */
    char headerBuffer[1024];
    size_t parsed = 0;
    int headerEnd = -1;

    while (headerEnd < 0){
        if (parsed == sizeof(headerBuffer)){
            request->status = GF_INVALID;
            close(sockfd);
            return -1;
        }

        ssize_t receiveHeader = recv(sockfd, headerBuffer + parsed, sizeof(headerBuffer) - parsed, 0);
        if (receiveHeader < 0){
            request->status = GF_ERROR;
            close(sockfd);
            return -1;
        }
        if (receiveHeader == 0){
            request->status = GF_INVALID;
            close(sockfd);
            return -1;
        }
        parsed+=(size_t)receiveHeader;
        headerEnd = find_header_tail(headerBuffer, parsed);
    }
    
    /* Go through the header */
    char headerContent[1024];
    size_t headerLen = (size_t)headerEnd;

    if (headerLen >= sizeof(headerContent)){
        request-> status = GF_INVALID;
        close(sockfd);
        return -1;
    }

    memcpy(headerContent, headerBuffer, headerLen);
    headerBuffer[headerLen] = '\0';
    
    char headerOne[16] = {0};
    char headerTwo[32] = {0};
    size_t fileLength = 0;

    int headerParser = sscanf(headerBuffer, "%15s %31s %zu", headerOne,headerTwo, &fileLength);

    if (headerParser < 2 || strcmp(headerOne, "GETFILE") != 0){
        request->status = GF_INVALID;
        close(sockfd);
        return -1;
    }

    if (strcmp(headerTwo, "OK") == 0){
        if (headerParser != 3){
            request->status = GF_INVALID;
            close(sockfd);
            return-1;
        }   
        request->status = GF_OK;
        request->file_len = fileLength;
    } else if (strcmp(headerTwo, "FILE_NOT_FOUND") == 0){
        request->status = GF_FILE_NOT_FOUND;
        request->file_len = 0;
        close(sockfd);
        return 0;
    } else if (strcmp(headerTwo, "ERROR") == 0){
        request->status = GF_ERROR;
        request->file_len = 0;
        close(sockfd);
        return 0;
    } else if (strcmp(headerTwo, "INVALID") == 0){
        request->status = GF_INVALID;
        request->file_len = 0;
        close(sockfd);
        return 0;
    } else {
        request->status = GF_INVALID;
        close(sockfd);
        return 0;
    }

    /* If the above passes, then we can begin to receive the bytes for the body of the file */
    request->bytes_received = 0;

    /* Check if there are any bytes after \r\n\r\n in the header */
    size_t postHeaderBytesRead = parsed - (size_t)headerEnd;
    if (postHeaderBytesRead > 0){
        size_t serverFileSize = postHeaderBytesRead;
        if (serverFileSize > request->file_len){
            serverFileSize = request->file_len;
        }
        if (serverFileSize > 0 && request->writefunc != NULL){
            request->writefunc(headerBuffer  + headerEnd, serverFileSize, request->writearg);
        }
        request->bytes_received+=serverFileSize;
    }
    
    /* Receive the body of the file */
    while (request->bytes_received < request->file_len){
        char bodyBuffer[4096];
        size_t remainingBytes = request->file_len - request->bytes_received;
        size_t requestedBytes;
        if (remainingBytes < sizeof(bodyBuffer)){
            requestedBytes = remainingBytes;
        } else {
            requestedBytes = sizeof(bodyBuffer);
        }

        ssize_t receiveBytes = recv(sockfd, bodyBuffer, requestedBytes, 0);
        if (receiveBytes <= 0){
            close(sockfd);
            return -1;
        }

        if (request->writefunc != NULL){
            request->writefunc(bodyBuffer, (size_t)receiveBytes, request->writearg);
        }
        request->bytes_received+=(size_t)receiveBytes;
    }
    close(sockfd);
    return 0;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->path = path;
    }
}
void gfc_set_server(gfcrequest_t **gfr, const char *server) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->server = server;
    }
}
void gfc_set_port(gfcrequest_t **gfr, unsigned short port) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->port = port;
    }
}
void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *)) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->headerfunc = headerfunc;
    }
}
void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->headerarg = headerarg;
    }
}
void gfc_set_writearg(gfcrequest_t **gfr, void *writearg) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->writearg = writearg;
    }
}
void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *)) {
    if (gfr != NULL && *gfr != NULL){
        (*gfr)->writefunc = writefunc;
    }
}
const char *gfc_strstatus(gfstatus_t status) {
  const char *strstatus = "UNKNOWN";

  switch (status) {

    case GF_OK: {
      strstatus = "OK";
    } break;

    case GF_FILE_NOT_FOUND: {
      strstatus = "FILE_NOT_FOUND";
    } break;

   case GF_INVALID: {
      strstatus = "INVALID";
    } break;
   
   case GF_ERROR: {
      strstatus = "ERROR";
    } break;

  }
  return strstatus;
}
