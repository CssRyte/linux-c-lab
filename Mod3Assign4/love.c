
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

int saura = 0;

void* helper(void* arg) {
    pthread_mutex_lock(&lock);

    saura = 1;

    pthread_cond_signal(&cv);

    pthread_mutex_unlock(&lock);

    return NULL;
}

int main() {
    pthread_t thread;

    pthread_create(&thread, NULL, helper, NULL);

    pthread_mutex_lock(&lock);

    while (saura != 1) {
        pthread_cond_wait(&cv, &lock);
    }

    if (saura == 1) {
        printf("I Love Emilia!\n");
    } else {
        printf("I Love Ken!\n");
    }

    pthread_mutex_unlock(&lock);

    pthread_join(thread, NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cv);

    return 0;
}
