#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;

    /* Maximum stack size */
    if (getrlimit(RLIMIT_STACK, &lim) == 0) {
        printf("stack size: %llu\n",
               (unsigned long long) lim.rlim_cur);
    } else {
        perror("getrlimit RLIMIT_STACK");
        return 1;
    }

    /* Maximum number of processes */
    if (getrlimit(RLIMIT_NPROC, &lim) == 0) {
        printf("process limit: %llu\n",
               (unsigned long long) lim.rlim_cur);
    } else {
        perror("getrlimit RLIMIT_NPROC");
        return 1;
    }

    /* Maximum number of open file descriptors */
    if (getrlimit(RLIMIT_NOFILE, &lim) == 0) {
        printf("max file descriptors: %llu\n",
               (unsigned long long) lim.rlim_cur);
    } else {
        perror("getrlimit RLIMIT_NOFILE");
        return 1;
    }

    return 0;
}