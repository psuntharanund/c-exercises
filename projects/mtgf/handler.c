#include "gfserver-student.h"
#include "gfserver.h"
#include "workload.h"
#include "content.h"
#include "steque.h"

pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t worker_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t boss_cv = PTHREAD_COND_INITIALIZER;

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

//
//  The purpose of this function is to handle a get request
//
//  The ctx is a pointer to the "context" operation and it contains connection state
//  The path is the path being retrieved
//  The arg allows the registration of context that is passed into this routine.
//  Note: you don't need to use arg. The test code uses it in some cases, but
//        not in others.
//
gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void* arg){
	
    struct job_t *job = calloc(1, sizeof(*job));
    steque_t *queue = calloc(1, sizeof(*));

    if (job == NULL){
        gfs_sendheader(ctx, GF_ERROR, 0);
        return gfh_failure;
    }

    job->ctx = ctx;
    job->path = strdup(path ? path : "");
    pthread_mutex_init(&job->done_mtx, NULL);
    pthread_cond_init(&job0->done_cv, NULL);
    job->complete = 0;
    job->result = gfh_failure;

    pthread_mutex_lock(&)
	return gfh_failure;
}

