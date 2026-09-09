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
        printf("[child] running ls -la\n");

        execlp("ls", "ls", "-la", (char *)NULL);

        perror("execlp");
        _exit(1);
    }

    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    printf("[parent] child finished\n");

    return 0;
}