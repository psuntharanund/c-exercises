#include "proxy-student.h"
#include "gfserver.h"

#define MAX_REQUEST_N 512
#define BUFSIZE (6426)

typedef struct{
    char *data;
    size_t size;
} MemoryBuf;

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
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    //perform xfer
    
    //xfer complete, get info
    
    //cleanup
	errno = ENOSYS;
	return -1;	
}

/*
 * We provide a dummy version of handle_with_file that invokes handle_with_curl as a convenience for linking!
 */
ssize_t handle_with_file(gfcontext_t *ctx, const char *path, void* arg){
	return handle_with_curl(ctx, path, arg);
}	
