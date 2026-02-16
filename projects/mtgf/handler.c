#include "gfserver-student.h"
#include "gfserver.h"
#include "workload.h"
#include "content.h"

static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cv = PTHREAD_COND_INITIALIZER;

static steque_t queue = {0};
static pthread_t *workers = NULL;
static int threadCount = 0;
static int queueingDone = 0;

typedef struct job{
    char *path;
    gfcontext_t *ctx;
}job_t;

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//
static int send_all(gfcontext_t **ctx, const char *buf, size_t len){
    size_t offset = 0;
    while (offset < len){
        ssize_t sent = gfs_send(ctx, buf + offset, len - offset);
        if (sent <= 0){
            return -1;
        }
        offset+=(size_t)sent;
    }
    return 0;
}

static void *server_worker_job(void *arg){
    (void)arg;
    for (;;){ 
        pthread_mutex_lock(&queue_mtx);
        while (steque_isempty(&queue) && queueingDone == 0){
            pthread_cond_wait(&queue_cv, &queue_mtx);
        }
        if (steque_isempty(&queue) && queueingDone != 0){
            pthread_mutex_unlock(&queue_mtx);
            break;
        }
        job_t *job = (job_t*)steque_pop(&queue);
        pthread_mutex_unlock(&queue_mtx);

        int send_abort = 0;
        int workfd = -1;
        int basefd = content_get(job->path);
        
        if (basefd < 0){
            if (gfs_sendheader(&job->ctx, GF_FILE_NOT_FOUND, 0) < 0){
                send_abort = 1;
            }
            goto done;
        }

        workfd = dup(basefd);
        if (workfd < 0){
            if (gfs_sendheader(&job->ctx, GF_ERROR, 0) < 0){
                send_abort = 1;
            }
            goto done;
        }

        struct stat fileStats;
        if (fstat(workfd, &fileStats) < 0){
            if (gfs_sendheader(&job->ctx, GF_FILE_NOT_FOUND, 0) < 0){
                send_abort = 1;
            }
            goto done;
        }
        size_t fileLen = (size_t)fileStats.st_size;
       
        if (gfs_sendheader(&job->ctx, GF_OK, fileLen) < 0){
            send_abort = 1;
            goto done;
        }

        char buffer[4096];
        off_t offset = 0;
 
        ssize_t bytesRead;
        while ((bytesRead = pread(workfd, buffer, sizeof(buffer), offset)) > 0){
            if (send_all(&job->ctx, buffer, (size_t)bytesRead) < 0){
                send_abort = 1;
                break;
            }    
            offset+=bytesRead;
        }
        
        if (bytesRead < 0){
            send_abort = 1;
        }

    done:
        if (workfd >= 0){
            close(workfd);
        }

        if (send_abort && job->ctx != NULL){
            gfs_abort(&job->ctx);
            job->ctx = NULL;
        }

        free(job->path);
        free(job);
    }
    return NULL;
}

void init_threads(size_t numthreads){
    signal(SIGPIPE, SIG_IGN);
    steque_init(&queue);
    threadCount = numthreads;
    workers = malloc(numthreads * sizeof(*workers));
    if (workers == 0){
        exit(1);
    }
    for (int i = 0; i < numthreads; i++){
        pthread_create(&workers[i], NULL, server_worker_job, NULL);
    }

}

void cleanup_threads(){
    pthread_mutex_lock(&queue_mtx);
    queueingDone = 1;
    pthread_cond_broadcast(&queue_cv);
    pthread_mutex_unlock(&queue_mtx);

    if (workers != 0){
        for (int i = 0; i < threadCount; i++){
            pthread_join(workers[i], NULL);
        }
            free(workers);
            workers = NULL;
    }

}

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
    (void)arg;

    job_t *job = calloc(1, sizeof(*job));
    if (job == NULL){
        gfs_sendheader(ctx, GF_ERROR, 0);
        *ctx = NULL;
        return gfh_failure;
    }

    job->ctx = *ctx;
    *ctx = NULL;
    job->path = strdup(path ? path : "");
    
    pthread_mutex_lock(&queue_mtx);
    steque_enqueue(&queue, (steque_item)job);
    pthread_cond_signal(&queue_cv);
    pthread_mutex_unlock(&queue_mtx);

	return gfh_success;
}
