#ifndef HANDLER_H
#define HANDLER_H

#include "gf-student.h"
#include "gfserver.h"

typedef struct job_t job_t;
typedef struct queue_t queue_t;
typedef struct gfcrequest_t gfcrequest_t;

void handler_set_threadCount(int workerpool);
int handler_init(void);
void handler_shutdown(void);

gfh_error_t handler_request(gfcontext_t **ctx, const char *path, void *arg);

#endif
