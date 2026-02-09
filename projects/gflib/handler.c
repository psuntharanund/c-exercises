#include "handler.h"

#define QUEUECAPACITY 20

pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t worker_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t boss_cv = PTHREAD_COND_INITIALIZER;

static int queueingDone = 0;
static int threadCount = 0;

struct job_t{
    int jobid;
    int sockfd; 
};

struct queue_t{
    job_t queue[QUEUECAPACITY];
    int qhead, qtail, qcount;
};

static queue_t q = {0};

/* Boss variable to let workers know that jobs are no longer being put into the queue */ 

static int queue_push(queue_t *q, job_t jobin){
    if (q->qcount == QUEUECAPACITY){
        return -1;
    }
    q->queue[q->qtail] = jobin;
    q->qtail = (q->qtail + 1) % QUEUECAPACITY;
    q->qcount++;
    return 0;
}

static int queue_pop(queue_t *q, job_t *jobout){
    if (q->qcount == 0){
        return -1;
    }
    *jobout = q->queue[q->qhead];
    q->qhead = (q->qhead + 1) % QUEUECAPACITY;
    q->qcount--;
    return 0;
}


void boss_job(int jobCount){
    for (int i = 1; i <= jobCount; i++){
        job_t jobin = {.jobid = i};
        pthread_mutex_lock(&queue_mtx);
            queue_push(&q, jobin);
            if (queue_push(&q, jobin) > QUEUECAPACITY){
                exit(1);
            }
    } 
    queueingDone = 1;
    pthread_mutex_unlock(&queue_mtx);
    pthread_cond_broadcast(&worker_cv);
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
        job_t job;
        queue_pop(&q, &job);
        pthread_mutex_unlock(&queue_mtx);
        pthread_cond_broadcast(&worker_cv);
    }
}

void handler_set_threadCount(int workerpool){
    if (workerpool > 0){
        threadCount = workerpool;
    }
}

int handler_init(void){
    pthread_t tids[threadCount];
    job_t jobs[threadCount];
    for (int i = 0; i <= threadCount; i++){
        pthread_create(&tids[i], NULL, worker_job, &jobs[i]);
    }
    return 0;
}

void handler_shutdown(){
    pthread_t tids[threadCount];
    for (int i = 0; i <=threadCount; i++){
        pthread_join(&tids[threadCount], NULL);
    }
}



