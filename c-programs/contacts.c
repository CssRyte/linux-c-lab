#include <stdio.h>

struct Contact {
    char name[50];
    int age;
};

void addContact() {
    struct Contact contact;

    printf("Enter name: ");
    scanf("%49s", contact.name);

    printf("Enter age: ");
    scanf("%d", &contact.age);

    FILE *file = fopen("contacts.txt", "a");

    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    fprintf(file, "%s %d\n", contact.name, contact.age);

    fclose(file);

    printf("Contact added successfully.\n");
}

void viewContacts() {
    struct Contact contact;

    FILE *file = fopen("contacts.txt", "r");

    if (file == NULL) {
        printf("No contacts found.\n");
        return;
    }

    printf("\nContact List:\n");

    while (fscanf(file, "%49s %d", contact.name, &contact.age) == 2) {
        printf("Name: %s | Age: %d\n", contact.name, contact.age);
    }

    fclose(file);
}

int main() {
    int choice;

    do {
        printf("\nContact Manager\n");
        printf("1. Add Contact\n");
        printf("2. View Contacts\n");
        printf("3. Exit\n");
        printf("Choose an option: ");

        scanf("%d", &choice);

        if (choice == 1) {
            addContact();
        } else if (choice == 2) {
            viewContacts();
        } else if (choice == 3) {
            printf("Goodbye!\n");
        } else {
            printf("Invalid option.\n");
        }

    } while (choice != 3);

    return 0;
}
