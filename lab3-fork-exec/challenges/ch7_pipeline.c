#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pattern>\n", argv[0]);
        return 1;
    }

    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t ls_pid = fork();

    if (ls_pid < 0) {
        perror("fork");
        return 1;
    }

    if (ls_pid == 0) {
        // First child: ls writes into the pipe
        close(fd[0]);

        if (dup2(fd[1], STDOUT_FILENO) == -1) {
            perror("dup2");
            _exit(1);
        }

        close(fd[1]);

        execlp("ls", "ls", (char *)NULL);

        perror("execlp ls");
        _exit(1);
    }

    pid_t grep_pid = fork();

    if (grep_pid < 0) {
        perror("fork");
        return 1;
    }

    if (grep_pid == 0) {
        // Second child: grep reads from the pipe
        close(fd[1]);

        if (dup2(fd[0], STDIN_FILENO) == -1) {
            perror("dup2");
            _exit(1);
        }

        close(fd[0]);

        execlp("grep", "grep", argv[1], (char *)NULL);

        perror("execlp grep");
        _exit(1);
    }

    // Parent does not use the pipe
    close(fd[0]);
    close(fd[1]);

    waitpid(ls_pid, NULL, 0);
    waitpid(grep_pid, NULL, 0);

    return 0;
}