#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t child1 = fork();

    if (child1 < 0) {
        printf("First fork failed!\n");
        return 1;
    }

    if (child1 == 0) {
        printf("I am child 1, PID: %d, Parent PID: %d\n",
               getpid(), getppid());
        return 0;
    }

    pid_t child2 = fork();

    if (child2 < 0) {
        printf("Second fork failed!\n");
        return 1;
    }

    if (child2 == 0) {
        printf("I am child 2, PID: %d, Parent PID: %d\n",
               getpid(), getppid());
        return 0;
    }

    printf("I am the parent, PID: %d\n", getpid());

    wait(NULL);
    wait(NULL);

    return 0;
}