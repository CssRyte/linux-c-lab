#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv)
{
    int pid, status;
    int newfd;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s output_file\n", argv[0]);
        exit(1);
    }

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(1);
    }

    if (pid == 0)
    {
        /* Child process */

        newfd = open(argv[1],
                     O_CREAT | O_TRUNC | O_WRONLY,
                     0644);

        if (newfd < 0)
        {
            perror("open");
            exit(1);
        }

        /* Redirect stdout to the output file */
        dup2(newfd, STDOUT_FILENO);

        close(newfd);

        /* Replace child with ls -l */
        execlp("ls", "ls", "-l", NULL);

        /* Only reached if exec fails */
        perror("execlp");
        exit(1);
    }
    else
    {
        /* Parent waits for child to finish */
        waitpid(pid, &status, 0);

        printf("all done\n");
    }

    return 0;
}