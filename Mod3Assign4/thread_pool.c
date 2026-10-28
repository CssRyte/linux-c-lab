
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4
#define NUM_TASKS 8
#define QUEUE_SIZE 8

typedef struct {
    void (*task)(int);
    int arg;
} Task;

typedef struct {
    Task tasks[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    int shutdown;

    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ThreadsSafeQueue;

void queue_init(ThreadsSafeQueue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = 0;

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_push(ThreadsSafeQueue *q, Task task) {
    pthread_mutex_lock(&q->lock);

    while (q->count == QUEUE_SIZE) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    q->tasks[q->tail] = task;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int queue_pop(ThreadsSafeQueue *q, Task *task) {
    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->count == 0 && q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }

    *task = q->tasks[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);

    return 1;
}

void* worker(void *arg) {
    ThreadsSafeQueue *q = (ThreadsSafeQueue*)arg;
    Task task;

    while (queue_pop(q, &task)) {
        task.task(task.arg);
    }

    return NULL;
}

void sample_task(int arg) {
    printf("Task %d executed by thread %lu\n",
           arg, (unsigned long)pthread_self());

    sleep(1);
}

int main(void) {
    ThreadsSafeQueue q;
    pthread_t threads[NUM_THREADS];

    queue_init(&q);

    // Create four reusable worker threads.
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, &q);
    }

    // Add eight tasks to the shared queue.
    for (int i = 0; i < NUM_TASKS; i++) {
        Task task = {sample_task, i + 1};
        queue_push(&q, task);
    }

    // No new tasks will be added.
    pthread_mutex_lock(&q.lock);
    q.shutdown = 1;
    pthread_cond_broadcast(&q.not_empty);
    pthread_mutex_unlock(&q.lock);

    // Wait for all workers to finish.
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nAll tasks completed successfully.\n");

    pthread_mutex_destroy(&q.lock);
    pthread_cond_destroy(&q.not_empty);
    pthread_cond_destroy(&q.not_full);

    return 0;
}
