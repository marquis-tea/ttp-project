#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    char username[50], password[50];
    int choice;

    printf("=== WELCOME TO THE ATM ===\n");
    printf("Username: ");
    scanf("%s", username);
    printf("Password: ");
    scanf("%s", password);

    // 1. AUTHENTICATION CALL
    // Here, you would typically call a function from database.c 
    // or run the database executable to verify.
    if (strcmp(username, "haqimi") == 0 && strcmp(password, "1234") == 0) {
        printf("\nLogin Successful!\n");
        
        while(1) {
            printf("\n1. Deposit\n2. Withdraw\n3. Exit\nSelect Option: ");
            scanf("%d", &choice);

            if (choice == 1) {
                // 2. RUNNING deposit.c (as a compiled binary)
                pid_t pid = fork();
                if (pid == 0) {
                    // This is the child process. Run the deposit program.
                    execl("./deposit", "./deposit", NULL);
                } else {
                    // Parent waits for the deposit process to finish
                    wait(NULL);
                }
            } else if (choice == 3) {
                break;
            }
        }
    } else {
        printf("Invalid Credentials.\n");
    }

    return 0;
}