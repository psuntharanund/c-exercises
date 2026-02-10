#include "handler.h"

#define QUEUECAPACITY 20

pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t worker_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t boss_cv = PTHREAD_COND_INITIALIZER;

static int queueingDone = 0;
static int threadCount = 0;
static pthread_t *workers = NULL;

struct job_t{
    int jobid;
    int sockfd; 
    char *path;
    gfcontext_t **ctx;
    gfh_error_t result;
    pthread_mutex_t done_mtx;
    pthread_cond_t done_cv;
    int complete;

};

struct queue_t{
    job_t *queue[QUEUECAPACITY];
    int qhead, qtail, qcount;
};

static queue_t q = {0};

/* Boss variable to let workers know that jobs are no longer being put into the queue */ 

static int queue_push(queue_t *q, job_t *jobin){
    if (q->qcount == QUEUECAPACITY){
        return -1;
    }
    q->queue[q->qtail] = jobin;
    q->qtail = (q->qtail + 1) % QUEUECAPACITY;
    q->qcount++;
    return 0;
}

static int queue_pop(queue_t *q, job_t **jobout){
    if (q->qcount == 0){
        return -1;
    }
    *jobout = q->queue[q->qhead];
    q->qhead = (q->qhead + 1) % QUEUECAPACITY;
    q->qcount--;
    return 0;
}

void *worker_job(void *arg){
    for (;;){
        pthread_mutex_lock(&queue_mtx);
        while (q.qcount == 0 && queueingDone != NULL){
            pthread_cond_wait (&worker_cv, &queue_mtx);
        }
        if (q.qcount == 0 && queueingDone == NULL){
            pthread_mutex_unlock(&queue_mtx);
            pthread_cond_signal((&boss_cv));
            break;
        } 
        job_t *job;
        queue_pop(&q, &job);
        pthread_mutex_unlock(&queue_mtx);
        pthread_cond_broadcast(&worker_cv);
    }
}

void handler_set_threadCount(int workerpool){
    if (workerpool < 0){
        threadCount = 1;
    }   else{
        threadCount = workerpool;
    }
}

int handler_init(void){
    workers = calloc((size_t)threadCount, sizeof(*workers));
    if (workers == NULL){
        return -1;
    }

    for (int i = 0; i <= threadCount; i++){
        if (pthread_create(&workers[i], NULL, worker_job, NULL) != 0){
            queueingDone = 1;
            for (int j = 0; j < i; j++){
                pthread_join(workers[j], NULL);
            }
                free(workers);
                workers = NULL;
                return -1;
        }
    }
    return 0;
}

void handler_shutdown(void){
    pthread_mutex_lock(&queue_mtx);
    queueingDone = 1;
    pthread_cond_broadcast(&worker_cv);
    pthread_cond_broadcast(&boss_cv);
    pthread_mutex_unlock(&queue_mtx);

    if (workers != NULL){
        for (int i = 0; i < threadCount; i++){
            pthread_join(workers[i], NULL);
            free(workers);
            workers = NULL;
        }
    }
}

gfh_error_t handler_request(gfcontext_t **ctx, const char *path, void *arg){
    
    job_t *job = calloc(1, sizeof(*job));

    if (job == NULL){
        gfs_sendheader(ctx, GF_ERROR, 0);
        gfs_abort(ctx);
    }

    job->ctx = ctx;
    job->path = strdup(path ? path : "");
    pthread_mutex_init(&job->done_mtx, NULL);
    pthread_cond_init(&job->done_cv, NULL);
    job->complete = 0;
    
    pthread_mutex_lock(&queue_mtx);
    while (queue_push(&q, job) != 0){
        pthread_mutex_unlock(&queue_mtx);
        pthread_mutex_lock(&queue_mtx);
    }

    pthread_cond_broadcast(&worker_cv);
    pthread_mutex_unlock(&queue_mtx);
    
    pthread_mutex_lock(&job->done_mtx);
    while (job->complete == NULL){
        pthread_cond_wait(&job->done_cv, &job->done_mtx);
    }
    pthread_mutex_unlock(&job->done_mtx);

    gfh_error_t result = job->result;

    pthread_cond_destroy(&job->done_cv);
    pthread_mutex_destroy(&job->done_mtx);
    free(job->path);
    free(job);

    return result;
}


