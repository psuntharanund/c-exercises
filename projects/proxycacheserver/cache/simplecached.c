#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <printf.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include "cache-student.h"
#include "shm_channel.h"
#include "simplecache.h"
#include "gfserver.h"

// CACHE_FAILURE
#if !defined(CACHE_FAILURE)
    #define CACHE_FAILURE (-1)
#endif 

#define MAX_CACHE_REQUEST_LEN 6112
#define MAX_SIMPLE_CACHE_QUEUE_SIZE 783

unsigned long int cache_delay;

static void _sig_handler(int signo){
	if (signo == SIGTERM || signo == SIGINT){
		// This is where your IPC clean up should occur
        cache_socket_cleanup();
        simplecache_destroy();
		exit(signo);
	}
}

ssize_t read_all(int fd, void *buf, size_t len){
    size_t totalBytes = 0;
    while (totalBytes < len){
        ssize_t sent = read(fd, (char*)buf + totalBytes, len - totalBytes);
        if (sent <= 0){
            return -1;
        }
        totalBytes+=sent;
    }
    return totalBytes;
}

#define USAGE                                                                 \
"usage:\n"                                                                    \
"  simplecached [options]\n"                                                  \
"options:\n"                                                                  \
"  -c [cachedir]       Path to static files (Default: ./)\n"                  \
"  -t [thread_count]   Thread count for work queue (Default is 8, Range is 1-100)\n"      \
"  -d [delay]          Delay in simplecache_get (Default is 0, Range is 0-2500000 (microseconds)\n "	\
"  -h                  Show this help message\n"

//OPTIONS
static struct option gLongOptions[] = {
  {"cachedir",           required_argument,      NULL,           'c'},
  {"nthreads",           required_argument,      NULL,           't'},
  {"help",               no_argument,            NULL,           'h'},
  {"hidden",			 no_argument,			 NULL,			 'i'}, /* server side */
  {"delay", 			 required_argument,		 NULL, 			 'd'}, // delay.
  {NULL,                 0,                      NULL,             0}
};

void Usage() {
  fprintf(stdout, "%s", USAGE);
}

int main(int argc, char **argv) {
	int nthreads = 6;
	char *cachedir = "locals.txt";
	char option_char;

	/* disable buffering to stdout */
	setbuf(stdout, NULL);

	while ((option_char = getopt_long(argc, argv, "d:ic:hlt:x", gLongOptions, NULL)) != -1) {
		switch (option_char) {
			default:
				Usage();
				exit(1);
			case 't': // thread-count
				nthreads = atoi(optarg);
				break;				
			case 'h': // help
				Usage();
				exit(0);
				break;    
            case 'c': //cache directory
				cachedir = optarg;
				break;
            case 'd':
				cache_delay = (unsigned long int) atoi(optarg);
				break;
			case 'i': // server side usage
			case 'o': // do not modify
			case 'a': // experimental
				break;
		}
	}

	if (cache_delay > 2500000) {
		fprintf(stderr, "Cache delay must be less than 2500000 (us)\n");
		exit(__LINE__);
	}

	if ((nthreads>100) || (nthreads < 1)) {
		fprintf(stderr, "Invalid number of threads must be in between 1-100\n");
		exit(__LINE__);
	}
	if (SIG_ERR == signal(SIGINT, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGINT...exiting.\n");
		exit(CACHE_FAILURE);
	}
	if (SIG_ERR == signal(SIGTERM, _sig_handler)){
		fprintf(stderr,"Unable to catch SIGTERM...exiting.\n");
		exit(CACHE_FAILURE);
	}
	/*Initialize cache*/
	simplecache_init(cachedir);

	// Cache should go here
    int cache_server_fd = cache_socket_init();
    if (cache_server_fd < 0){
        fprintf(stderr, "Failed to initialize cache socket.\n");
        exit(CACHE_FAILURE);
    }

    while (1){
        //request specific resources
        int clientfd = accept(cache_server_fd, NULL, NULL);
        int shmfd = -1;
        int filefd = -1;
        void *addr = MAP_FAILED;
        struct stat st;
        cache_request_t request;
        shm_packet_t *pkt = NULL;
        sem_t *sem_empty = SEM_FAILED;
        sem_t *sem_full = SEM_FAILED; 
        size_t headerSize = sizeof(shm_packet_t);
        size_t dataCap = 0;


        if (clientfd < 0){
            continue;
        }

        if (read_all(clientfd, &request, sizeof(request)) < 0){
            close(clientfd);
            continue;
        }

        shmfd = shm_open(request.shm_name, O_RDWR, 0600);
        if (shmfd < 0){
            close(clientfd);
            continue;
        }

        addr = mmap(NULL, request.seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
        if (addr == MAP_FAILED){
            close(shmfd);
            close(clientfd);
            continue;
        }

        pkt = (shm_packet_t *)addr;
        dataCap = request.seg_size - headerSize;
        
        sem_empty = sem_open(request.sem_is_empty, 0);
        sem_full = sem_open(request.sem_is_full, 0);

        if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED){
            munmap(addr, request.seg_size);
            close(shmfd);
            close(clientfd);
            continue;
        }

        filefd = simplecache_get(request.path);
        int reqfd = dup(filefd);
        if (reqfd < 0){
            goto send_fail;
        } else {
            if (fstat(reqfd, &st) < 0){
                goto send_fail;
            } else{
                size_t totalBytesTransferred = 0;
                size_t fileSize = (size_t)st.st_size;

                while (1){
                    ssize_t response;
                    sem_wait(sem_empty);
                    response = pread(reqfd, pkt->buffer, dataCap, totalBytesTransferred);
                    if (response < 0){
                        pkt->status = CACHE_STATUS_NOTFOUND;
                        pkt->fileSize = fileSize;
                        pkt->validBytes = 0;
                        pkt->streamDone = 1;
                        sem_post(sem_full);
                        break;
                    }

                    pkt->status = CACHE_STATUS_OK;
                    pkt->fileSize = fileSize;
                    pkt->validBytes = (size_t)response;
                    totalBytesTransferred+=(size_t)response;
                    pkt->streamDone = (totalBytesTransferred >= fileSize || response == 0);
                    sem_post(sem_full);

                    if (pkt->streamDone){
                        break;
                    }
                }
            }
            goto cleanup;

    send_fail:
        sem_wait(sem_empty);
        pkt->status = CACHE_STATUS_NOTFOUND;
        pkt->fileSize = 0;
        pkt->validBytes = 0;
        pkt->streamDone = 1;
        sem_post(sem_full);
    
    cleanup:
        if (filefd >= 0){
            close(filefd);
        }

        if (sem_empty != SEM_FAILED){
            sem_close(sem_empty);
        }

        if (sem_full != SEM_FAILED){
            sem_close(sem_full);
        }

        if (addr != MAP_FAILED){
            munmap(addr, request.seg_size);
        }

        if (shmfd >= 0){
            close(shmfd);
        }

        if (clientfd >= 0){
            close(clientfd);
        }
    }
    }
	// Line never reached
	return -1;

}
