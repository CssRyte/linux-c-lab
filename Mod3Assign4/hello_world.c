
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t lock;
pthread_cond_t cv;
int hello = 0;

void* print_hello(void* arg) {
    pthread_mutex_lock(&lock);

    hello = 1;
    printf("First line (hello=%d)\n", hello);

    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main() {
    pthread_t thread;

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cv, NULL);

    pthread_create(&thread, NULL, print_hello, NULL);

    pthread_mutex_lock(&lock);

    while (hello != 1) {
        pthread_cond_wait(&cv, &lock);
    }

    printf("Second line (hello=%d)\n", hello);

    pthread_mutex_unlock(&lock);

    pthread_join(thread, NULL);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cv);

    return 0;
}
