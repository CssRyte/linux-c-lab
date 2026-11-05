#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 2
#define ITERATIONS 100000

int counter = 0;

// Create a mutex
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Function executed by each thread
void* increment_counter(void* arg) {

    for (int i = 0; i < ITERATIONS; i++) {

       pthread_mutex_lock(&lock);

counter++;

pthread_mutex_unlock(&lock);
    }

    return NULL;
}

int main() {

    pthread_t threads[NUM_THREADS];

    // Create two threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL,
                       increment_counter, NULL);
    }

    // Wait for both threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Expected counter: %d\n",
           NUM_THREADS * ITERATIONS);

    printf("Actual counter: %d\n", counter);

    pthread_mutex_destroy(&lock);

    return 0;
}