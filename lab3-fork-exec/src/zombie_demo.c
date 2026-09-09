#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        printf("[child] PID=%d exiting now\n", getpid());
        _exit(0);
    }

    printf("[parent] PID=%d created child PID=%d\n", getpid(), pid);
    printf("[parent] sleeping for 10 seconds...\n");

    sleep(10);

    waitpid(pid, NULL, 0);

    printf("[parent] child collected; zombie removed\n");

    return 0;
}