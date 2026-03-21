#include "gfserver.h"
#include "cache-student.h"
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>


#define BUFSIZE (839)

/*
 __.__
Replace with your implementation
 __.__
*/
ssize_t handle_with_cache(gfcontext_t *ctx, const char *path, void* arg){
    proxy_pool_t *pool = (proxy_pool_t *)arg;
    proxy_seg_t *seg = NULL;
    shm_packet_t *pkt;
    cache_request_t request;
    int sock = -1;
    ssize_t totalBytesTransferred = 0;
    int headerTransferred = 0;
    
    //each request needs their own segment
    seg = proxy_seg_acq(pool);
    pkt = (shm_packet_t *)seg->addr;

    //since segments are reusable, we need to clear it each time a worker picks up a segment
    memset(pkt, 0, seg->seg_size);
    memset(&request, 0, sizeof(request));

    //populate what gets sent to command channel
    strncpy(request.path, path, sizeof(request.path) -1);
    strncpy(request.shm_name, seg->shm_name, sizeof(request.shm_name) -1);
    strncpy(request.sem_is_empty, seg->sem_is_empty, sizeof(request.sem_is_empty) - 1);
    strncpy(request.sem_is_full, seg->sem_is_full, sizeof(request.sem_is_full) -1);
    request.seg_size = seg->seg_size;

    //connect to cache 
    //they should not crash if other process hasn't started yet
    //"if proxy cannot connect to IPC mechanism . . .then it might delay for a second and try again"
    while ((sock = connect_to_cache_socket()) < 0){
        sleep(1);
    }

    if (write(sock, &request, sizeof(request)) != sizeof(request)){
        goto failure;
    }

    while (1){
        if (sem_wait(seg->sem_full) < 0){
            goto failure;
        }
        //we only need to be able to distinguish between cache hit or miss
        if (!headerTransferred){
            if (pkt->status == CACHE_STATUS_NOTFOUND){
                sem_post(seg->sem_empty);
                close(sock);
                proxy_seg_release(pool, seg);
                return gfs_sendheader(ctx, GF_FILE_NOT_FOUND, 0);
            } else if (pkt->status != CACHE_STATUS_OK){
                sem_post(seg->sem_empty);
                goto failure;
            }

            if (gfs_sendheader(ctx, GF_OK, pkt->fileSize) < 0){
                sem_post(seg->sem_empty);
                goto failure;
            }

            headerTransferred = 1;
        }

        if (pkt->validBytes > 0){
            ssize_t sent = gfs_send(ctx, pkt->buffer, pkt->validBytes);
            if (sent != (ssize_t)pkt->validBytes){
                sem_post(seg->sem_empty);
                goto failure;
            }
            totalBytesTransferred+=sent;
        }
        
        if (pkt->streamDone){
            sem_post(seg->sem_empty);
            break;
        }

        sem_post(seg->sem_empty);
    }

    close(sock);
    proxy_seg_release(pool, seg);
    return totalBytesTransferred;

failure:
    close(sock);
    proxy_seg_release(pool, seg);
    return SERVER_FAILURE;
}
