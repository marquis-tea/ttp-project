// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Tell the compiler this function exists in database.c
extern int check_credentials(char *user, char *pass);

int main() {
    char username[50], password[50];
    int choice;

    printf("=== ATM CLIENT UI ===\n");
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    // Call the function from database.c
    if (check_credentials(username, password)) {
        printf("\nLogin Successful!\n");

        while (1) {
            printf("\n1. Deposit\n2. Exit\nSelection: ");
            scanf("%d", &choice);

            if (choice == 1) {
                // This runs the compiled deposit program
                system("./deposit"); 
            } else if (choice == 2) {
                printf("Exiting...\n");
                break;
            }
        }
    } else {
        printf("\nInvalid Username or Password.\n");
    }

    return 0;
}