#include "gfserver-student.h"
#include "gfserver.h"
#include "workload.h"
#include "content.h"
#include "steque.h"

static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cv = PTHREAD_COND_INITIALIZER;

static steque_t queue = {0};
static pthread_t *workers = NULL;

typedef struct job{
    int jobid;
    int sockfd; 
    char *path;
    gfcontext_t **ctx;
    gfh_error_t result;
    pthread_mutex_t done_mtx;
    pthread_cond_t done_cv;
    int complete;

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
void *worker_job(){

}

void init_threads(size_t numthreads){
    steque_init(&queue);

    for (int i = 0; i < numthreads; i++){
        pthread_create(&workers[i], NULL, worker_job, NULL);
    }

};

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
	
    job_t *job = calloc(1, sizeof(*job));
    
    if (job == NULL){
        gfs_sendheader(ctx, GF_ERROR, 0);
        return gfh_failure;
    }
    
    job->ctx = ctx;
    job->path = strdup(path ? path : "");
    pthread_mutex_init(&job->done_mtx, NULL);
    pthread_cond_init(&job->done_cv, NULL);
    job->complete = 0;
    job->result = gfh_failure;
    
    pthread_mutex_lock(&queue_mtx);
    steque_push(&queue, (steque_item)job);

    pthread_mutex_unlock(&queue_mtx);
    pthread_cond_broadcast(&queue_cv);

    pthread_mutex_lock(&job->done_mtx);
    while (job->complete == 0){
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

