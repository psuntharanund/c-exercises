// In case you want to implement the shared memory IPC as a library
// This is optional but may help with code reuse
//
#include "cache-student.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <errno.h>

static int g_cache_sockfd = -1;

int proxy_pool_init(proxy_pool_t *pool, unsigned int nsegments, size_t segment_size){
    memset(pool, 0, sizeof(*pool));

    pool->segment = calloc(nsegments, sizeof(proxy_seg_t));
    if (!pool->segment){
        return -1;
    }

    pool->seg_count = nsegments;
    pool->seg_size = segment_size;

    pthread_mutex_init(&pool->m, NULL);
    pthread_cond_init(&pool->cv, NULL);
    
    for (unsigned int i = 0; i < nsegments; i++){
        proxy_seg_t *seg = &pool->segment[i];
        
        //this is so that each proxy process gets their own unique namespace
        snprintf(seg->shm_name, sizeof(seg->shm_name), "/proxy_shm_%d_%u", getpid(), i);
        snprintf(seg->sem_is_empty, sizeof(seg->sem_is_empty), "/proxy_empty_%d_%u", getpid(), i);
        snprintf(seg->sem_is_full, sizeof(seg->sem_is_full), "/proxy_full_%d_%u", getpid(), i);

        seg->seg_size = segment_size;
        seg->inUse = 0;
        
        //open shmem object
        seg->shmfd = shm_open(seg->shm_name, O_CREAT | O_RDWR, 0600);
        if (seg->shmfd < 0){
            return -1;
        }
        
        //establish length of shmem object
        if (ftruncate(seg->shmfd, segment_size) < 0){
            return -1;
        }
        
        //map shmrm object into proxy process for worker
        seg->addr = mmap(NULL, segment_size, PROT_READ | PROT_WRITE, MAP_SHARED, seg->shmfd, 0);
        if (seg->addr == MAP_FAILED){
            return -1;
        }

        //implement semaphores for handshake between proxy workers and shmem segment 1 for available, 0 for in use
        seg->sem_empty = sem_open(seg->sem_is_empty, O_CREAT, 0600, 1);
        if (seg->sem_empty == SEM_FAILED){
            return -1;
        }

        seg->sem_full = sem_open(seg->sem_is_full, O_CREAT, 0600, 0);
        if (seg->sem_full == SEM_FAILED){
            return -1;
        }
    }
    return 0;
}

void proxy_pool_destroy(proxy_pool_t *pool){
    if (!pool || !pool->segment){
        return;
    }

    //since each segment will own several resources, we must cleanup the pool per segment 
    for (unsigned int i = 0; i < pool->seg_count; i++){
        proxy_seg_t *seg = &pool->segment[i];

        if (seg->addr && seg->addr != MAP_FAILED){
            munmap(seg->addr, seg->seg_size);
        }
        
        //remove the process's handle
        if (seg->shmfd >= 0){
            close(seg->shmfd);
        }

        if (seg->sem_empty && seg->sem_empty != SEM_FAILED){
            sem_close(seg->sem_empty);
        }

        if (seg->sem_full && seg->sem_full != SEM_FAILED){
            sem_close(seg->sem_full);
        }
        
        //remove the object
        shm_unlink(seg->shm_name);
        sem_unlink(seg->sem_is_empty);
        sem_unlink(seg->sem_is_full);
    }

    free(pool->segment);
    pthread_mutex_destroy(&pool->m);
    pthread_cond_destroy(&pool->cv);
}

proxy_seg_t *proxy_seg_acq(proxy_pool_t *pool){
    pthread_mutex_lock(&pool->m);

    while (1){
        for (unsigned int i = 0; i < pool->seg_count; i++){
            if (!pool->segment[i].inUse){
                pool->segment[i].inUse = 1;
                pthread_mutex_unlock(&pool->m);
                return &pool->segment[i];
            }
        }
        pthread_cond_wait(&pool->cv, &pool->m);
    }
}

void proxy_seg_release(proxy_pool_t *pool, proxy_seg_t *seg){
    pthread_mutex_lock(&pool->m);
    seg->inUse = 0;
    pthread_cond_signal(&pool->cv);
    pthread_mutex_unlock(&pool->m);
}

int cache_socket_init(){
    struct sockaddr_un addr;

    unlink(CACHE_SOCK_PATH);
    g_cache_sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_cache_sockfd < 0){
        return -1;
    }
    
    //setup socket endpoint for IPC
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CACHE_SOCK_PATH, sizeof(addr.sun_path) - 1);
    
    //associates socket path with good path, so proxies know where to connect
    if (bind(g_cache_sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        return -1;
    }
    
    //mark socket as passive so incoming client connections can be accepted
    if (listen(g_cache_sockfd, 64) < 0){
        return -1;
    }

    return g_cache_sockfd;
}

void cache_socket_cleanup(){
    if (g_cache_sockfd >= 0){
        close(g_cache_sockfd);
    }
    unlink(CACHE_SOCK_PATH);
}

int connect_to_cache_socket(){
    int pfd;
    struct sockaddr_un addr;

    pfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (pfd < 0){
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, CACHE_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(pfd, (struct sockaddr *)&addr, sizeof(addr)) < 0){
        close(pfd);
        return -1;
    }

    return pfd;
}


