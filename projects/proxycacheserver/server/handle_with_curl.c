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
    if (cbptr){
        return 0;
    }

    memory->data = cbptr;
    memcpy(&(memory->data[memory->size]), body, totalSize);
    memory->size += totalSize;
    memory->data[memory->size] = '\0';

    return totalSize;
}
ssize_t handle_with_curl(gfcontext_t *ctx, const char *path, void* arg){
	(void) ctx;
	(void) arg;
	(void) path;
	//Your implementation here
    CURL *curl;
    CURLcode result;
    char error_buf[CURL_ERROR_SIZE];
    MemoryBuf response;
    response.data = malloc(1);
    response.size = 0;

    if (!response.data){
        fprintf(stderr, "Malloc failed.\n");
        return 1;
    }

    response.data[0] = '\0';

    //init easy-session 

    curl = curl_easy_init();
    if (!curl){
        fprintf(stderr, "Curl easy init failed.\n");
        curl_global_cleanup();
        free(response.data);
        return 1;
    }
    
    error_buf[0] = '\0';

    //set options
    curl_easy_setopt(curl, CURLOPT_URL, "https://raw.githubusercontent.com/gt-cs6200/image_data/master/yellowstone.jpg");
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buf);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    //perform xfer
    result = curl_easy_perform(curl);
    if (result != CURLE_OK){
        fprintf(stderr, "Easy perform failed.\n", error_buf[0] ? error_buf : curl_easy_strerror(result));
    } else {
        int responseCode = 0;
        char *responseContent = NULL;

        //xfer complete, get info
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &responseContent);

    }
    
    //cleanup
    curl_easy_cleanup(curl);
    free(response.data);
	return 0;	
}

/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl as a convenience for linking!
 */
ssize_t handle_with_file(gfcontext_t *ctx, const char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}	
