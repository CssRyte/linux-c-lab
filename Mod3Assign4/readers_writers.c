
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t reader_cv = PTHREAD_COND_INITIALIZER;
pthread_cond_t writer_cv = PTHREAD_COND_INITIALIZER;

int reader_count = 0;
int writer_waiting = 0;
int writer_active = 0;
int shared_data = 0;

void* reader(void* arg) {
    int id = *(int*)arg;

    pthread_mutex_lock(&lock);

    // Wait if a writer is active or waiting.
    while (writer_active || writer_waiting > 0) {
        pthread_cond_wait(&reader_cv, &lock);
    }

    reader_count++;

    printf("Reader %d starts reading. Active readers: %d\n",
           id, reader_count);

    pthread_mutex_unlock(&lock);

    // Multiple readers may read simultaneously.
    printf("Reader %d reads: %d\n", id, shared_data);

    usleep(100000);

    pthread_mutex_lock(&lock);

    reader_count--;

    printf("Reader %d finished reading.\n", id);

    if (reader_count == 0 && writer_waiting > 0) {
        pthread_cond_signal(&writer_cv);
    }

    pthread_mutex_unlock(&lock);

    return NULL;
}

void* writer(void* arg) {
    int id = *(int*)arg;

    pthread_mutex_lock(&lock);

    writer_waiting++;

    // Wait until no readers or writers are active.
    while (reader_count > 0 || writer_active) {
        pthread_cond_wait(&writer_cv, &lock);
    }

    writer_waiting--;
    writer_active = 1;

    shared_data++;

    printf("Writer %d writes: %d\n",
           id, shared_data);

    writer_active = 0;

    // Give priority to another waiting writer.
    if (writer_waiting > 0) {
        pthread_cond_signal(&writer_cv);
    } else {
        pthread_cond_broadcast(&reader_cv);
    }

    pthread_mutex_unlock(&lock);

    return NULL;
}

int main() {
    pthread_t readers[3], writers[2];

    int reader_ids[3] = {1, 2, 3};
    int writer_ids[2] = {1, 2};

    for (int i = 0; i < 3; i++) {
        pthread_create(&readers[i], NULL,
                       reader, &reader_ids[i]);
    }

    for (int i = 0; i < 2; i++) {
        pthread_create(&writers[i], NULL,
                       writer, &writer_ids[i]);
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }

    printf("\nFinal shared data: %d\n", shared_data);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&reader_cv);
    pthread_cond_destroy(&writer_cv);

    return 0;
}
