// In case you want to implement the shared memory IPC as a library
// This is optional but may help with code reuse
//
#include "cache-student.h"

static int g_cache_sockfd = -1;

int proxy_pool_init(proxy_pool_t *pool, unsigned int nsegments, size_t segment_size){

}

void proxy_pool_destroy(proxy_pool_t *pool){

}

proxy_seg_t *proxy_seg_acq(proxy_pool_t *pool){

}

void proxy_seg_release(proxy_pool_t *pool, proxy_seg_t *segment){

}

int cache_socket_init(){

}

void cache_socket_cleanup(){

}

int connect_to_cache_socket(){

}


