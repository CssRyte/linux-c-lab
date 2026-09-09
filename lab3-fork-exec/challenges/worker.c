#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    printf("worker: arguments received:\n");

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = %s\n", i, argv[i]);
    }

    char *myvar = getenv("MYVAR");

    if (myvar != NULL) {
        printf("MYVAR=%s\n", myvar);
    } else {
        printf("MYVAR is not set\n");
    }

    return 0;
}