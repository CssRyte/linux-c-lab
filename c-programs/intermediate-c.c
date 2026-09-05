#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main() {
    int num;

    printf("Enter a number: ");
    scanf("%d", &num);

    if (num > 0) {
        printf("Positive\n");
    } else {
        printf("Non-positive\n");
    }

    printf("Loop:\n");

    for (int i = 1; i <= 5; i++) {
        printf("%d\n", i);
    }

    printf("3 + 4 = %d\n", add(3, 4));

    return 0;
}
