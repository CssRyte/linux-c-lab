#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

int ready = 0;

void* consumer(void* arg) {
    (void)arg;

    pthread_mutex_lock(&mutex);

    while (!ready) {
        printf("Consumer waiting...\n");
        pthread_cond_wait(&condition, &mutex);
    }

    printf("Consumer received signal!\n");

    pthread_mutex_unlock(&mutex);

    return NULL;
}

void* producer(void* arg) {
    (void)arg;

    sleep(2);

    pthread_mutex_lock(&mutex);

    ready = 1;
    printf("Producer set ready = 1\n");

    pthread_cond_signal(&condition);

    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main() {
    pthread_t producerThread;
    pthread_t consumerThread;

    pthread_create(&consumerThread, NULL, consumer, NULL);
    pthread_create(&producerThread, NULL, producer, NULL);

    pthread_join(consumerThread, NULL);
    pthread_join(producerThread, NULL);

    pthread_cond_destroy(&condition);
    pthread_mutex_destroy(&mutex);

    return 0;
}