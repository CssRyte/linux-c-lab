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
        printf(
            "Hello from child (PID=%d, PPID=%d)\n",
            getpid(),
            getppid()
        );

        sleep(2);

        _exit(7);
    }

    int status;

    pid_t child = waitpid(pid, &status, 0);

    if (child == -1) {
        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status)) {
        printf(
            "child %d exited with status %d\n",
            child,
            WEXITSTATUS(status)
        );
    }

    return 0;
}