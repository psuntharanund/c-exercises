#include "proxy-student.h"
#include "gfserver.h"

#define MAX_REQUEST_N 512
#define BUFSIZE (6426)

typedef struct{
    char *data;
    size_t size;
} MemoryBuf;

static size_t write_callback(void *body, size_t size, size_t mem, void *user){
    size_t totalSize = size * mem;
    MemoryBuf *memory = (MemoryBuf *)user;

    char *cbptr = realloc(memory->data, memory->size + totalSize + 1);
    if (!cbptr){
        return 0;
    }

    memory->data = cbptr;
    memcpy(&(memory->data[memory->size]), body, totalSize);
    memory->size += totalSize;
    memory->data[memory->size] = '\0';

    return totalSize;
}

static ssize_t send_all(gfcontext_t *ctx, void *buf, size_t len){
    size_t totalBytesSent = 0;
    char *ptr = (char *)buf;

    while (totalBytesSent < len){
        ssize_t sent = gfs_send(ctx, ptr + totalBytesSent, len - totalBytesSent);
        if (sent <= 0){
            return -1;
        }
        totalBytesSent+=sent;
    }
    return (ssize_t)totalBytesSent;
}

ssize_t handle_with_curl(gfcontext_t *ctx, const char *path, void* arg){
	(void) ctx;
	(void) arg;
	(void) path;
	//Your implementation here
    CURL *curl;
    CURLcode result;
    MemoryBuf response;
    char url[2048];
    const char *urlBase = (const char *)arg;
    response.data = malloc(1);
    response.size = 0;
    
    if (!ctx || !path || !urlBase){
        return -1;
    }

    snprintf(url, sizeof(url), "%s%s", urlBase, path);

    if (!response.data){
        fprintf(stderr, "Malloc failed.\n");
        return 1;
    }

    response.data[0] = '\0';

    //init easy-session 
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK){
        fprintf(stderr, "Global init failed.\n");
        free(response.data);
        return 1;
    }

    curl = curl_easy_init();
    if (!curl){
        fprintf(stderr, "Curl easy init failed.\n");
        curl_global_cleanup();
        free(response.data);
        return 1;
    } 

    //set options
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    //perform xfer
    result = curl_easy_perform(curl);
    if (result != CURLE_OK){
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        curl_easy_cleanup(curl);
        free(response.data);
        return 1;

    }

    int responseCode = 0;
    char *responseContent = NULL;

    //xfer complete, get info
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &responseContent);

    if (responseCode == 404){
        gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
        curl_easy_cleanup(curl);
        free(response.data);
        return 1;
    }

    if (responseCode >= 400){
        gfs_sendheader(ctx, GF_ERROR, 0);
        curl_easy_cleanup(url);
        free(response.data);
        return 1;
    }

    if (gfs_sendheader(ctx, GF_OK, response.size) < 0){
        curl_easy_cleanup(curl);
        free(response.data);
        return 1;
    }

    if (response.size > 0){
        if (send_all(ctx, response.data, response.size) < 0){
            curl_easy_cleanup(curl);
            free(response.data);
            return 1;
        }
    }
    
    //cleanup
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    free(response.data);
	return (ssize_t)response.size;	
}

/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl as a convenience for linking!
 */
ssize_t handle_with_file(gfcontext_t *ctx, const char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}	
