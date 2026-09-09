#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t first = fork();

    if (first < 0) {
        perror("fork");
        return 1;
    }

    if (first == 0) {
        // First child: ls -> pipe
        close(fd[0]);

        dup2(fd[1], STDOUT_FILENO);

        close(fd[1]);

        execlp("ls", "ls", (char *)NULL);

        perror("execlp ls");
        _exit(1);
    }

    pid_t second = fork();

    if (second < 0) {
        perror("fork");
        return 1;
    }

    if (second == 0) {
        // Second child: pipe -> grep
        close(fd[1]);

        dup2(fd[0], STDIN_FILENO);

        close(fd[0]);

        execlp("grep", "grep", "demo", (char *)NULL);

        perror("execlp grep");
        _exit(1);
    }

    // Parent closes both pipe ends
    close(fd[0]);
    close(fd[1]);

    waitpid(first, NULL, 0);
    waitpid(second, NULL, 0);

    printf("[parent] pipeline finished\n");

    return 0;
}