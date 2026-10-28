
#include <stdio.h>
#include <pthread.h>

#define TOTAL_CAKES 3

int numBatterInBowl = 0;
int numEggInBowl = 0;
int readyToEat = 0;
int cakesEaten = 0;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t needIngredients = PTHREAD_COND_INITIALIZER;
pthread_cond_t readyToBake = PTHREAD_COND_INITIALIZER;
pthread_cond_t startEating = PTHREAD_COND_INITIALIZER;

void addBatter() {
    numBatterInBowl = 1;
    printf("Batter added to bowl.\n");
}

void addEgg() {
    numEggInBowl = 1;
    printf("Egg added to bowl.\n");
}

void heatBowl() {
    printf("Heating bowl... Cake baked!\n");
    readyToEat = 1;
}

void eatCake() {
    printf("Cake eaten! Total: %d\n", cakesEaten + 1);

    readyToEat = 0;
    numBatterInBowl = 0;
    numEggInBowl = 0;
    cakesEaten++;
}

void* batterAdder(void* arg) {
    pthread_mutex_lock(&lock);

    while (cakesEaten < TOTAL_CAKES) {

        while (numBatterInBowl == 1 &&
               cakesEaten < TOTAL_CAKES) {
            pthread_cond_wait(&needIngredients, &lock);
        }

        if (cakesEaten >= TOTAL_CAKES)
            break;

        addBatter();

        pthread_cond_signal(&readyToBake);
    }

    pthread_mutex_unlock(&lock);
    return NULL;
}

void* eggBreaker(void* arg) {
    pthread_mutex_lock(&lock);

    while (cakesEaten < TOTAL_CAKES) {

        while (numEggInBowl == 1 &&
               cakesEaten < TOTAL_CAKES) {
            pthread_cond_wait(&needIngredients, &lock);
        }

        if (cakesEaten >= TOTAL_CAKES)
            break;

        addEgg();

        pthread_cond_signal(&readyToBake);
    }

    pthread_mutex_unlock(&lock);
    return NULL;
}

void* bowlHeater(void* arg) {
    pthread_mutex_lock(&lock);

    while (cakesEaten < TOTAL_CAKES) {

        while ((numBatterInBowl < 1 ||
                numEggInBowl < 1 ||
                readyToEat) &&
               cakesEaten < TOTAL_CAKES) {
            pthread_cond_wait(&readyToBake, &lock);
        }

        if (cakesEaten >= TOTAL_CAKES)
            break;

        heatBowl();

        pthread_cond_signal(&startEating);
    }

    pthread_mutex_unlock(&lock);
    return NULL;
}

void* cakeEater(void* arg) {
    pthread_mutex_lock(&lock);

    while (cakesEaten < TOTAL_CAKES) {

        while (!readyToEat) {
            pthread_cond_wait(&startEating, &lock);
        }

        eatCake();

        pthread_cond_broadcast(&needIngredients);
        pthread_cond_signal(&readyToBake);
    }

    // Wake all remaining threads so they can terminate.
    pthread_cond_broadcast(&needIngredients);
    pthread_cond_signal(&readyToBake);

    pthread_mutex_unlock(&lock);
    return NULL;
}

int main() {
    pthread_t batter, egg1, heater, eater;

    pthread_create(&batter, NULL, batterAdder, NULL);
    pthread_create(&egg1, NULL, eggBreaker, NULL);
    pthread_create(&heater, NULL, bowlHeater, NULL);
    pthread_create(&eater, NULL, cakeEater, NULL);

    pthread_join(batter, NULL);
    pthread_join(egg1, NULL);
    pthread_join(heater, NULL);
    pthread_join(eater, NULL);

    printf("\nBaking completed: %d cakes eaten.\n",
           cakesEaten);

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&needIngredients);
    pthread_cond_destroy(&readyToBake);
    pthread_cond_destroy(&startEating);

    return 0;
}
