#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid > 0) {
        printf("I am the parent, PID: %d\n", getpid());
        wait(NULL);
    }
    else if (pid == 0) {
        printf("I am the child, PID: %d, Parent PID: %d\n",
               getpid(), getppid());

        pid_t grandchild = fork();

        if (grandchild > 0) {
            wait(NULL);
        }
        else if (grandchild == 0) {
            printf("I am the grandchild, PID: %d, Parent PID: %d\n",
                   getpid(), getppid());
        }
        else {
            printf("Grandchild fork failed!\n");
        }
    }
    else {
        printf("Fork failed!\n");
    }

    return 0;
}