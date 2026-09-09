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
        // Child process
        printf("[child] PID=%d, PPID=%d\n", getpid(), getppid());

        sleep(1);

        printf("[child] exiting with code 42\n");

        _exit(42);
    } else {
        // Parent process
        printf("[parent] created child PID=%d\n", pid);

        int status;

        pid_t w = waitpid(pid, &status, 0);

        if (w == -1) {
            perror("waitpid");
            return 1;
        }

        if (WIFEXITED(status)) {
            printf(
                "[parent] child %d exited with status %d\n",
                w,
                WEXITSTATUS(status)
            );
        } else {
            printf("[parent] child %d did not exit normally\n", w);
        }
    }

    return 0;
}