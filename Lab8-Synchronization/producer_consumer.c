#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define BUFFER_SIZE 5
#define NUM_ITEMS 10

int buffer[BUFFER_SIZE];
int in = 0;
int out = 0;

sem_t emptySlots;
sem_t fullSlots;
sem_t mutex;

void* producer(void* arg) {
    (void)arg;

    for (int i = 1; i <= NUM_ITEMS; i++) {
        sem_wait(&emptySlots);
        sem_wait(&mutex);

        buffer[in] = i;
        printf("Produced: %d\n", i);
        in = (in + 1) % BUFFER_SIZE;

        sem_post(&mutex);
        sem_post(&fullSlots);

        usleep(100000);
    }

    return NULL;
}

void* consumer(void* arg) {
    (void)arg;

    for (int i = 1; i <= NUM_ITEMS; i++) {
        sem_wait(&fullSlots);
        sem_wait(&mutex);

        int item = buffer[out];
        printf("Consumed: %d\n", item);
        out = (out + 1) % BUFFER_SIZE;

        sem_post(&mutex);
        sem_post(&emptySlots);

        usleep(200000);
    }

    return NULL;
}

int main() {
    pthread_t producerThread;
    pthread_t consumerThread;

    sem_init(&emptySlots, 0, BUFFER_SIZE);
    sem_init(&fullSlots, 0, 0);
    sem_init(&mutex, 0, 1);

    pthread_create(
        &producerThread, NULL, producer, NULL
    );

    pthread_create(
        &consumerThread, NULL, consumer, NULL
    );

    pthread_join(producerThread, NULL);
    pthread_join(consumerThread, NULL);

    sem_destroy(&emptySlots);
    sem_destroy(&fullSlots);
    sem_destroy(&mutex);

    return 0;
}