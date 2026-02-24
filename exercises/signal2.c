#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(void){
    sigset_t block_set, old_set;

    sigemptyset(&block_set);
    sigaddset(&block_set, SIGINT);

    printf("Blocking SIGINT for 5 seconds. . .\n");

    sigprocmask(SIG_BLOCK, &block_set, &old_set);
    sleep(5);

    printf("Unblocking SIGINT\n");

    sigprocmask(SIG_SETMASK, &old_set, NULL);

    sleep(5);
    return 0;
}
