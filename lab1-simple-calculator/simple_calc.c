#include <stdio.h>
#include <stdlib.h>

// Function prototypes
double add(double a, double b);
double subtract(double a, double b);
double multiply(double a, double b);
double divide(double a, double b);

int main(int argc, char *argv[]) {

    // Check if the correct number of arguments was entered
    if (argc != 4) {
        printf("Usage: ./simple_calc <number1> <operator> <number2>\n");
        return 1;
    }

    // Convert command-line arguments
    double number1 = atof(argv[1]);
    char operator = argv[2][0];
    double number2 = atof(argv[3]);

    double result;

    // Perform the requested calculation
    switch (operator) {
        case '+':
            result = add(number1, number2);
            break;

        case '-':
            result = subtract(number1, number2);
            break;

        case '*':
            result = multiply(number1, number2);
            break;

        case '/':
            if (number2 == 0) {
                printf("Error: Division by zero is not allowed.\n");
                return 1;
            }

            result = divide(number1, number2);
            break;

        default:
            printf("Error: Unsupported operator '%c'. Use +, -, *, or /.\n", operator);
            return 1;
    }

    printf("Result: %.2f\n", result);

    return 0;
}

// Function definitions
double add(double a, double b) {
    return a + b;
}

double subtract(double a, double b) {
    return a - b;
}

double multiply(double a, double b) {
    return a * b;
}

double divide(double a, double b) {
    return a / b;
}