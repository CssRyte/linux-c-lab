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
        printf("[child] PID=%d exiting immediately\n", getpid());
        _exit(0);
    }

    printf("[parent] PID=%d created child PID=%d\n", getpid(), pid);
    printf("[parent] sleeping 10 seconds before wait()\n");

    sleep(10);

    waitpid(pid, NULL, 0);

    printf("[parent] wait complete; zombie removed\n");

    return 0;
}