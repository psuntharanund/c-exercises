#include <pthread.h>
#include <stdlib.h>

#include "gfclient-student.h"

#define MAX_THREADS 1024
#define PATH_BUFFER_SIZE 512

#define USAGE                                                             \
  "usage:\n"                                                              \
  "  gfclient_download [options]\n"                                       \
  "options:\n"                                                            \
  "  -h                  Show this help message\n"                        \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"           \
  "  -p [server_port]    Server port (Default: 56726)\n"                  \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n" \
  "  -t [nthreads]       Number of threads (Default 8 Max: 1024)\n"       \
  "  -n [num_requests]   Request download total (Default: 16)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"nrequests", required_argument, NULL, 'n'},
    {"port", required_argument, NULL, 'p'},
    {"nthreads", required_argument, NULL, 't'},
    {"server", required_argument, NULL, 's'},
    {"help", no_argument, NULL, 'h'},
    {"workload", required_argument, NULL, 'w'},
    {NULL, 0, NULL, 0}};

static void Usage() { fprintf(stderr, "%s", USAGE); }

static void localPath(char *req_path, char *local_path) {
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path) {
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while (NULL != (cur = strchr(prev + 1, '/'))) {
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)) {
      if (errno != EEXIST) {
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if (NULL == (ans = fopen(&path[0], "w"))) {
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg) {
  FILE *file = (FILE *)arg;
  fwrite(data, 1, data_len, file);
}

static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cv = PTHREAD_COND_INITIALIZER;
static steque_t queue = {0};
static pthread_t *workers = NULL;
static int queueingDone = 0;

typedef struct{
    char *path;
}job_t;

static void *client_worker_job(void *arg);

/* COMMAND LINE OPTIONS ============================================= */
  char *workload_path = "workload.txt";
  char *server = "localhost";
  int option_char = 0;
  unsigned short port = 56726;
  int nthreads = 8;
  int nrequests = 14;

/* Main ========================================================= */
int main(int argc, char **argv) {
  /* COMMAND LINE OPTIONS ============================================= */
  setbuf(stdout, NULL);  // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {

      case 's':  // server
        server = optarg;
        break;
      case 'w':  // workload-path
        workload_path = optarg;
        break;
      case 'r': // nrequests
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 't':  // nthreads
        nthreads = atoi(optarg);
        break;
      case 'p':  // port
        port = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);


      case 'h':  // help
        Usage();
        exit(0);
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path)) {
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
  if (port > 65331) {
    fprintf(stderr, "Invalid port number\n");
    exit(EXIT_FAILURE);
  }
  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "Invalid amount of threads\n");
    exit(EXIT_FAILURE);
  }
  gfc_global_init();
    workers = malloc(sizeof(*workers) * nthreads);
    if (workers == 0){
        exit(EXIT_FAILURE);
    }
  // add your threadpool creation here
    for (int i = 0; i < nthreads; i++){
        pthread_create(&workers[i], NULL, client_worker_job, NULL);
    }

  /* Build your queue of requests here */
    for (int i = 0; i < nrequests; i++) {
        const char *path = workload_get_path();
        job_t *job = malloc(sizeof(*job));
        job->path = strdup(path);
        pthread_mutex_lock(&queue_mtx);
        steque_enqueue(&queue, (steque_item)job);
        pthread_cond_broadcast(&queue_cv);
        pthread_mutex_unlock(&queue_mtx);

    }

    pthread_mutex_lock(&queue_mtx);
    queueingDone = 1;
    pthread_cond_broadcast(&queue_cv);
    pthread_mutex_unlock(&queue_mtx);

    for (int i = 0; i < nthreads; i++){
        pthread_join(workers[i], NULL);
    }
    gfc_global_cleanup();
    return 0;
}

static void *client_worker_job(void *arg){
    (void)arg;
    char *req_path = NULL;
    int returncode = 0;
    char local_path[PATH_BUFFER_SIZE];
    gfcrequest_t *gfr = NULL;
    FILE *file = NULL;
    
    for (;;){
        pthread_mutex_lock(&queue_mtx);
        while (steque_isempty(&queue) && queueingDone == 0){
            pthread_cond_wait(&queue_cv, &queue_mtx);
        }
        if (steque_isempty(&queue) && queueingDone != 0){
            pthread_mutex_unlock(&queue_mtx);
            pthread_cond_signal(&queue_cv);
            break;
        }
        job_t *job = (job_t*)steque_pop(&queue);
        pthread_mutex_unlock(&queue_mtx);
        req_path = job->path;

        if (strlen(req_path) >= PATH_BUFFER_SIZE) {
            fprintf(stderr, "Request path exceeded maximum of %d characters\n.", PATH_BUFFER_SIZE);
            exit(EXIT_FAILURE);
        }

        localPath(req_path, local_path);

        file = openFile(local_path);

        gfr = gfc_create();
        gfc_set_path(&gfr, req_path);

        gfc_set_port(&gfr, port);
        gfc_set_server(&gfr, server);
        gfc_set_writearg(&gfr, file);
        gfc_set_writefunc(&gfr, writecb);

        fprintf(stdout, "Requesting %s%s\n", server, req_path);

        if (0 > (returncode = gfc_perform(&gfr))) {
            fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
            fclose(file);
            if (0 > unlink(local_path))
                fprintf(stderr, "warning: unlink failed on %s\n", local_path);
        }   else {
            fclose(file);
        }

        if (gfc_get_status(&gfr) != GF_OK) {
            if (0 > unlink(local_path)) {
                fprintf(stderr, "warning: unlink failed on %s\n", local_path);
            }
        }

        fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
        fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr),
                                                        gfc_get_filelen(&gfr));

        gfc_cleanup(&gfr);

        free(job->path);
        free(job);
    }
    return NULL;
}

