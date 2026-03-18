/*
You can use this however you want.
*/
#ifndef __CACHE_STUDENT_H__844

#define __CACHE_STUDENT_H__844

#include "steque.h"
#include "gfserver.h"

#define CACHE_SOCK_PATH "/tmp/simplecache.sock"
#define MAX_REQ_PATH 512
#define MAX_NAME 64

#define CACHE_STATUS_OK 0
#define CACHE_STATUS_NOTFOUND 1

//message from proxy to the cache
typedef struct{
    char path[MAX_REQ_PATH];
    char shm_name[MAX_NAME];
    char sem_is_empty[MAX_NAME];
    char sem_is_full[MAX_NAME];
    size_t seg_size;
} cache_request_t;

//data channel(shared memory) packet format
typedef struct{
    int status;
    size_t fileSize;
    size_t validBytes;
    int streamDone;
    char buffer[];
} shm_packet_t;

//proxy owned shmem segment
typedef struct{
    char shm_name[MAX_NAME];
    char sem_is_empty[MAX_NAME];
    char sem_is_full[MAX_NAME];
    int shm_fd;
    void *addr;
    size_t seg_size;
    sem_t *sem_empty;
    sem_t *sem_full;
    int inUse;
} proxy_seg_t;

//proxy segment manager since webproxy is multithreaded
typedef struct{
    proxy_seg_t *segment;
    ssize_t seg_count;
    size_t seg_size;
    pthread_mutex_t m;
    pthread_cond_t cv;
} proxy_pool_t;


#endif // __CACHE_STUDENT_H__844
