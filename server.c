#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define ACCOUNTS_FILE "accounts.txt"
/* Return codes */
#define OK       0
#define ERR_ID   1
#define ERR_PIN  2
#define ERR_FILE 3

int verify_login(char* id, char* pin)
{
    int fd;
    char buffer[100];
    char username[50];
    char account_id[20];
    char stored_pin[20];
    char balance[20];
    int i, field, pos;
    int bytes_read;
    int input_id, input_pin;
    int stored_id_num, stored_pin_num;
    
/*    // Validate ID contains only digits 
    for (i = 0; id[i] != '\0'; i++) {
        if (id[i] < '0' || id[i] > '9') {
            printf("Error: ID must contain only digits\n");
            return ERR_ID;
        }
    }
    
    /* Validate PIN contains only digits 
    for (i = 0; pin[i] != '\0'; i++) {
        if (pin[i] < '0' || pin[i] > '9') {
            printf("Error: PIN must contain only digits\n");
            return ERR_PIN;
        }
    } */
    
    /* Convert input ID and PIN to integers */
    input_id = atoi(id);
    input_pin = atoi(pin);
    
    /* open() accounts.txt */
    fd = open(ACCOUNTS_FILE, O_RDONLY);
    if (fd < 0) {
        perror("open error");
        return ERR_FILE;
    }
    
    /* Read and parse file line by line */
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        
        /* Parse the line: Username|AccountID|PIN|Balance */
        field = 0;
        pos = 0;
        
        /* Reset arrays */
        for (i = 0; i < 50; i++) username[i] = '\0';
        for (i = 0; i < 20; i++) account_id[i] = '\0';
        for (i = 0; i < 20; i++) stored_pin[i] = '\0';
        for (i = 0; i < 20; i++) balance[i] = '\0';
        
        /* Parse each field separated by | */
        for (i = 0; i < bytes_read && buffer[i] != '\n'; i++) {
            if (buffer[i] == '|') {
                field++;
                pos = 0;
            } else {
                if (field == 0) {
                    username[pos++] = buffer[i];
                } else if (field == 1) {
                    account_id[pos++] = buffer[i];
                } else if (field == 2) {
                    stored_pin[pos++] = buffer[i];
                } else if (field == 3) {
                    balance[pos++] = buffer[i];
                }
            }
        }
        
        /* Convert stored AccountID and PIN to integers */
        stored_id_num = atoi(account_id);
        stored_pin_num = atoi(stored_pin);
        
        /* Compare AccountID as integers */
        if (stored_id_num == input_id) {
            close(fd);
            
            /* Compare PIN as integers */
            if (stored_pin_num != input_pin) {
                printf("Error: Invalid PIN\n");
                return ERR_PIN;
            }
            
            printf("Authentication successful\n");
            return OK;
        } else {
			 printf("Error: Invalid ID\n");
             return ERR_ID;
			
        
        /* Move to next line if exists */
        while (i < bytes_read && buffer[i] != '\n') {
            i++;
        }
        if (i < bytes_read - 1) {
            lseek(fd, -(bytes_read - i - 1), SEEK_CUR);
        }
    }
    
    /* Account ID not found */
    close(fd);
    printf("Error: Invalid Account ID\n");
    return ERR_ID;
}

int main(int argc, char *argv[])
{
    /* Expect: program AccountID PIN */
    if (argc != 3) {
        printf("Usage: %s AccountID PIN\n", argv[0]);
        return ERR_FILE;
    }
    
    return verify_login(argv[1], argv[2]);
}
