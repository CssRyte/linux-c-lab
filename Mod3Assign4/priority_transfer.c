
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

typedef struct {
    pthread_mutex_t lock;
    int balance;
    int uid;
} account_t;

account_t acc1 = {PTHREAD_MUTEX_INITIALIZER, 1000, 1};
account_t acc2 = {PTHREAD_MUTEX_INITIALIZER, 500, 2};

pthread_mutex_t state_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t state_cv = PTHREAD_COND_INITIALIZER;

int low_has_lock = 0;
int donation_attempted = 0;

atomic_int low_effective_priority = 1;

void move_money(account_t *donor, account_t *recipient,
                int amount) {
    if (amount > 0 && donor->balance >= amount) {
        donor->balance -= amount;
        recipient->balance += amount;

        printf("Transferred %d from account %d to account %d\n",
               amount, donor->uid, recipient->uid);
    } else {
        puts("Transfer rejected.");
    }
}

void *low_thread(void *arg) {
    (void)arg;

    pthread_mutex_lock(&acc1.lock);
    puts("Low (priority 1): acquired account 1 lock");

    pthread_mutex_lock(&state_lock);
    low_has_lock = 1;
    pthread_cond_broadcast(&state_cv);

    while (!donation_attempted) {
        pthread_cond_wait(&state_cv, &state_lock);
    }
    pthread_mutex_unlock(&state_lock);

    printf("Low: effective priority = %d\n",
           atomic_load(&low_effective_priority));

    pthread_mutex_lock(&acc2.lock);
    move_money(&acc1, &acc2, 100);
    pthread_mutex_unlock(&acc2.lock);

    pthread_mutex_unlock(&acc1.lock);

    atomic_store(&low_effective_priority, 1);
    puts("Low: released locks; restored priority 1");

    return NULL;
}

void *high_thread(void *arg) {
    (void)arg;

    pthread_mutex_lock(&state_lock);

    while (!low_has_lock) {
        pthread_cond_wait(&state_cv, &state_lock);
    }
    pthread_mutex_unlock(&state_lock);

    if (pthread_mutex_trylock(&acc1.lock) != 0) {
        puts("High (priority 5): lock held by low thread");

        atomic_store(&low_effective_priority, 5);
        puts("High: donated priority 5 (simulation)");

        pthread_mutex_lock(&state_lock);
        donation_attempted = 1;
        pthread_cond_signal(&state_cv);
        pthread_mutex_unlock(&state_lock);

        pthread_mutex_lock(&acc1.lock);
    }

    pthread_mutex_lock(&acc2.lock);
    move_money(&acc2, &acc1, 50);
    pthread_mutex_unlock(&acc2.lock);
    pthread_mutex_unlock(&acc1.lock);

    puts("High: transfer completed");

    return NULL;
}

int main(void) {
    pthread_t low, high;

    pthread_create(&low, NULL, low_thread, NULL);
    pthread_create(&high, NULL, high_thread, NULL);

    pthread_join(low, NULL);
    pthread_join(high, NULL);

    printf("Final balances: account 1 = %d; "
           "account 2 = %d\n",
           acc1.balance, acc2.balance);

    pthread_cond_destroy(&state_cv);
    pthread_mutex_destroy(&state_lock);
    pthread_mutex_destroy(&acc1.lock);
    pthread_mutex_destroy(&acc2.lock);

    return 0;
}
