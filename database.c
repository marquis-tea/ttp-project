#include "common.h"

int verify_user(const char *target_id, const char *input_pin) {
    int fd = open("accounts.txt", O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return false;
    }
    char ch;
    char current_field[MAX_FIELD];
    int idx = 0;
    
    // Flags to track where we are in the line
    // 0 = Reading AccountID
    // 1 = Skipping Username
    // 2 = Reading PIN
    // 3 = Skipping rest of line (Balance, etc.)
    int state = 0; 
    
    while (read(fd, &ch, 1) > 0) {
        
        // --- STATE 0: Read AccountID ---
        if (state == 0) {
            if (ch == '|') {
                current_field[idx] = '\0'; // Null-terminate ID
                idx = 0; // Reset buffer index
                
                // Check if this is the correct user
                if (strcmp(current_field, target_id) == 0) {
                    state = 1; // ID Match: Move to skip Username
                } else {
                    state = 3; // ID Mismatch: Skip to next line
                }
            } else if (ch == '\n') {
                // Unexpected end of line, reset
                idx = 0;
                state = 0;
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 1: Skip Username ---
        else if (state == 1) {
            if (ch == '|') {
                // Username skipped. Now ready to read PIN.
                idx = 0; // **FIX: Reset index before reading PIN**
                state = 2; 
            }
            // Ignore all other chars in username
        }
        
        // --- STATE 2: Read PIN ---
        else if (state == 2) {
            if (ch == '|' || ch == '\n') {
                current_field[idx] = '\0'; // Null-terminate PIN
                
                // We found the ID *and* the PIN. Check it now.
                if (strcmp(current_field, input_pin) == 0) {
                    close(fd);
                    return true; // SUCCESS
                } else {
                    close(fd);
                    return false; // Wrong PIN
                }
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 3: Skip Rest of Line ---
        else if (state == 3) {
            if (ch == '\n') {
                // End of row reached. Reset to start looking for next ID.
                state = 0;
                idx = 0;
            }
        }
    }
    
    close(fd);
    return 1; // User not found
}

//==================================================================================
//check balance

int check_balance(const char *target_id, char *balance_output) {
    int fd = open("accounts.txt", O_RDONLY);
    if (fd == -1) {
        perror("Error opening file");
        return false;
    }
    
    char ch;
    char current_field[MAX_FIELD];
    int idx = 0;
    
    // Flags to track where we are in the line
    // 0 = Reading AccountID
    // 1 = Skipping Username
    // 2 = Skipping PIN
    // 3 = Reading Balance
    // 4 = Skipping rest of line
    int state = 0;
    
    while (read(fd, &ch, 1) > 0) {
        
        // --- STATE 0: Read AccountID ---
        if (state == 0) {
            if (ch == '|') {
                current_field[idx] = '\0'; // Null-terminate ID
                idx = 0; // Reset buffer index
                
                // Check if this is the correct user
                if (strcmp(current_field, target_id) == 0) {
                    state = 1; // ID Match: Move to skip Username
                } else {
                    state = 4; // ID Mismatch: Skip to next line
                }
            } else if (ch == '\n') {
                // Unexpected end of line, reset
                idx = 0;
                state = 0;
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 1: Skip Username ---
        else if (state == 1) {
            if (ch == '|') {
                // Username skipped. Now skip PIN.
                state = 2;
            }
        }
        
        // --- STATE 2: Skip PIN ---
        else if (state == 2) {
            if (ch == '|') {
                // PIN skipped. Now ready to read Balance.
                idx = 0; // Reset index before reading Balance
                state = 3;
            }
        }
        
        // --- STATE 3: Read Balance ---
        else if (state == 3) {
            if (ch == '|' || ch == '\n') {
                current_field[idx] = '\0'; // Null-terminate Balance
                
                // Copy balance to output
                strcpy(balance_output, current_field);
                close(fd);
                return true; // SUCCESS
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 4: Skip Rest of Line ---
        else if (state == 4) {
            if (ch == '\n') {
                // End of row reached. Reset to start looking for next ID.
                state = 0;
                idx = 0;
            }
        }
    }
    
    close(fd);
    return false; // Account not found
}

//==================================================================================
//deposit

// Union for semaphore operations
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Lock semaphore
void sem_lock(int semid) {
    struct sembuf sb = {0, -1, 0}; // Wait operation
    semop(semid, &sb, 1);
}

// Unlock semaphore
void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, 0}; // Signal operation
    semop(semid, &sb, 1);
}

int deposit_amount(const char *target_id, const char *deposit_amt) {
    // Step 1: Get or create semaphore
    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("Error creating semaphore");
        return 0;
    }
    
    // Initialize semaphore to 1 (unlocked)
    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);
    
    // Step 2: Lock the file
    sem_lock(semid);
    
    // Step 3: Open accounts.txt for reading and writing
    int fd = open("accounts.txt", O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        sem_unlock(semid);
        return 0;
    }
    
    char ch;
    char current_field[MAX_FIELD];
    int idx = 0;
    int account_found = 0;
    off_t balance_position = 0;
    
    // State machine
    // 0 = Reading AccountID
    // 1 = Skipping Username
    // 2 = Skipping PIN
    // 3 = Reading Balance
    // 4 = Skipping rest of line
    int state = 0;
    
    while (read(fd, &ch, 1) > 0) {
        
        // --- STATE 0: Read AccountID ---
        if (state == 0) {
            if (ch == '|') {
                current_field[idx] = '\0';
                idx = 0;
                
                if (strcmp(current_field, target_id) == 0) {
                    account_found = 1;
                    state = 1;
                } else {
                    state = 4;
                }
            } else if (ch == '\n') {
                idx = 0;
                state = 0;
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 1: Skip Username ---
        else if (state == 1) {
            if (ch == '|') {
                state = 2;
            }
        }
        
        // --- STATE 2: Skip PIN ---
        else if (state == 2) {
            if (ch == '|') {
                idx = 0;
                balance_position = lseek(fd, 0, SEEK_CUR); // Save position
                state = 3;
            }
        }
        
        // --- STATE 3: Read Balance ---
        else if (state == 3) {
            if (ch == '|' || ch == '\n') {
                current_field[idx] = '\0';
                
                // Calculate new balance
                int old_balance = atoi(current_field);
                int deposit = atoi(deposit_amt);
                int new_balance = old_balance + deposit;
                
                // Convert new balance to string
                char new_balance_str[MAX_FIELD];
                int new_len = snprintf(new_balance_str, MAX_FIELD, "%d", new_balance);
                int old_len = strlen(current_field);
                
                // Seek back to balance position and write new balance
                lseek(fd, balance_position, SEEK_SET);
                write(fd, new_balance_str, new_len);
                
                // If new balance is shorter, pad with spaces
                for (int i = new_len; i < old_len; i++) {
                    write(fd, " ", 1);
                }
                
                close(fd);
                sem_unlock(semid);
                return 1; // Success
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 4: Skip rest of line ---
        else if (state == 4) {
            if (ch == '\n') {
                state = 0;
                idx = 0;
            }
        }
    }
    
    // Close file and unlock
    close(fd);
    sem_unlock(semid);
    
    return account_found;
}

//==================================================================================
//withdrawal

// Union for semaphore operations
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Lock semaphore
void sem_lock(int semid) {
    struct sembuf sb = {0, -1, 0}; // Wait operation
    semop(semid, &sb, 1);
}

// Unlock semaphore
void sem_unlock(int semid) {
    struct sembuf sb = {0, 1, 0}; // Signal operation
    semop(semid, &sb, 1);
}

int withdraw_amount(const char *target_id, const char *withdraw_amt, char *new_balance_output) {
    // Step 1: Get or create semaphore
    int semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("Error creating semaphore");
        return 0;
    }
    
    // Initialize semaphore to 1 (unlocked)
    union semun arg;
    arg.val = 1;
    semctl(semid, 0, SETVAL, arg);
    
    // Step 2: Lock the file
    sem_lock(semid);
    
    // Step 3: Open accounts.txt for reading and writing
    int fd = open("accounts.txt", O_RDWR);
    if (fd == -1) {
        perror("Error opening file");
        sem_unlock(semid);
        return 0;
    }
    
    char ch;
    char current_field[MAX_FIELD];
    int idx = 0;
    int account_found = 0;
    off_t balance_position = 0;
    
    // State machine
    // 0 = Reading AccountID
    // 1 = Skipping Username
    // 2 = Skipping PIN
    // 3 = Reading Balance
    // 4 = Skipping rest of line
    int state = 0;
    
    while (read(fd, &ch, 1) > 0) {
        
        // --- STATE 0: Read AccountID ---
        if (state == 0) {
            if (ch == '|') {
                current_field[idx] = '\0';
                idx = 0;
                
                if (strcmp(current_field, target_id) == 0) {
                    account_found = 1;
                    state = 1;
                } else {
                    state = 4;
                }
            } else if (ch == '\n') {
                idx = 0;
                state = 0;
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 1: Skip Username ---
        else if (state == 1) {
            if (ch == '|') {
                state = 2;
            }
        }
        
        // --- STATE 2: Skip PIN ---
        else if (state == 2) {
            if (ch == '|') {
                idx = 0;
                balance_position = lseek(fd, 0, SEEK_CUR); // Save position
                state = 3;
            }
        }
        
        // --- STATE 3: Read Balance ---
        else if (state == 3) {
            if (ch == '|' || ch == '\n') {
                current_field[idx] = '\0';
                
                // Check if balance >= withdrawal amount
                int current_balance = atoi(current_field);
                int withdraw = atoi(withdraw_amt);
                
                if (current_balance < withdraw) {
                    // Insufficient balance
                    close(fd);
                    sem_unlock(semid);
                    return -1; // Return -1 for insufficient balance
                }
                
                // Calculate new balance
                int new_balance = current_balance - withdraw;
                
                // Convert new balance to string
                char new_balance_str[MAX_FIELD];
                int new_len = snprintf(new_balance_str, MAX_FIELD, "%d", new_balance);
                int old_len = strlen(current_field);
                
                // Copy new balance to output
                strcpy(new_balance_output, new_balance_str);
                
                // Seek back to balance position and write new balance
                lseek(fd, balance_position, SEEK_SET);
                write(fd, new_balance_str, new_len);
                
                // If new balance is shorter, pad with spaces
                for (int i = new_len; i < old_len; i++) {
                    write(fd, " ", 1);
                }
                
                close(fd);
                sem_unlock(semid);
                return 1; // Success
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 4: Skip rest of line ---
        else if (state == 4) {
            if (ch == '\n') {
                state = 0;
                idx = 0;
            }
        }
    }
    
    // Close file and unlock
    close(fd);
    sem_unlock(semid);
    
    return 0; // Account not found
}

