
#include <stdio.h>
#include <pthread.h>

typedef struct account_t {
    pthread_mutex_t lock;
    int balance;
    int uid;
} account_t;

void transfer(account_t *donor, account_t *recipient,
              float amount) {

    // Always lock the lower-ID account first.
    account_t *first =
        (donor->uid < recipient->uid) ? donor : recipient;

    account_t *second =
        (donor->uid < recipient->uid) ? recipient : donor;

    pthread_mutex_lock(&first->lock);

    if (first != second) {
        pthread_mutex_lock(&second->lock);
    }

    if (amount <= 0 || amount > donor->balance) {
        printf("Insufficient funds or invalid amount.\n");
    } else {
        donor->balance -= (int)amount;
        recipient->balance += (int)amount;

        printf("Transferred %.0f from account %d to %d\n",
               amount, donor->uid, recipient->uid);
    }

    if (first != second) {
        pthread_mutex_unlock(&second->lock);
    }

    pthread_mutex_unlock(&first->lock);
}

typedef struct {
    account_t *donor;
    account_t *recipient;
    float amount;
} transfer_args;

void* transfer_thread(void *arg) {
    transfer_args *args = (transfer_args *)arg;

    transfer(args->donor, args->recipient, args->amount);

    return NULL;
}

int main() {
    account_t acc1 = {
        PTHREAD_MUTEX_INITIALIZER, 1000, 1
    };

    account_t acc2 = {
        PTHREAD_MUTEX_INITIALIZER, 500, 2
    };

    pthread_t t1, t2;

    transfer_args args1 = {&acc1, &acc2, 200};
    transfer_args args2 = {&acc2, &acc1, 100};

    pthread_create(&t1, NULL, transfer_thread, &args1);
    pthread_create(&t2, NULL, transfer_thread, &args2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final account 1 balance: %d\n", acc1.balance);
    printf("Final account 2 balance: %d\n", acc2.balance);

    pthread_mutex_destroy(&acc1.lock);
    pthread_mutex_destroy(&acc2.lock);

    return 0;
}
