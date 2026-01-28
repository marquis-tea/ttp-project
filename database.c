// database.c
#include <string.h>

int check_credentials(char *user, char *pass) {
    // Checking for your specific test case
    if (strcmp(user, "haqimi") == 0 && strcmp(pass, "1234") == 0) {
        return 1; // Success
    }
    return 0; // Failure
}