#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void handle_sigint(int signo){
    (void)signo;
    write(STDOUT_FILENO, "\nCaught SIGINT (Ctrl-C)\n", 24);
}

int main(void){
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigint;

    sigaction(SIGINT, &sa, NULL);

    while (1){
        printf("Running. . . press Ctrl-C\n");
        sleep(1);
    }
}
