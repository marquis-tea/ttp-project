#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define MAX_FIELD 50
#define SEM_KEY 1234

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void p(int semid) {
    struct sembuf p_buf;
    p_buf.sem_num = 0;
    p_buf.sem_op = -1; /* Set the operation to be -1 which is locking */
    p_buf.sem_flg = SEM_UNDO;

    if ( semop (semid, &p_buf, 1) == -1){
        perror("p() failed");
        exit(1);
    }
    return;
}

void v(int semid) {
    struct sembuf v_buf;
    v_buf.sem_num = 0;
    v_buf.sem_op = 1; /* Set the operation to be +1 which is unlocking */
    v_buf.sem_flg = SEM_UNDO;

    if ( semop (semid, &v_buf, 1) == -1){
        perror("v() failed");
        exit(1);
    }
    return;
}

int verify_user(int fd, const char *target_id, const char *input_pin) {
    char ch;
    char current_field[MAX_FIELD];
    int idx = 0;
    
    // Flags to track where we are in the line
    // 0 = Reading AccountID
    // 1 = Skipping Username
    // 2 = Reading PIN
    // 3 = Skipping rest of line
    int state = 0; 
    
    lseek(fd, 0, SEEK_SET);
    
    while (read(fd, &ch, 1) > 0) {
        
        // --- STATE 0: Read AccountID ---
        if (state == 0) {
            if (ch == '|') {
                current_field[idx] = '\0';
                idx = 0; // Reset buffer index
                
                // Check if this is the correct user
                if (strcmp(current_field, target_id) == 0) {
                    state = 1; // If ID matches, then go skip username and check PIN
                } else {
                    state = 3; // If ID mismatch, skip the whole line
                }
	    } else if(ch == '\n' || ch == '\r') {
	    	idx = 0;
            } else {
                if (idx < MAX_FIELD - 1) current_field[idx++] = ch;
            }
        }
        
        // --- STATE 1: Skip Username ---
        else if (state == 1) {
            if (ch == '|') {
                idx = 0;
                state = 2; 
            }
        }
        
        // --- STATE 2: Read PIN ---
        else if (state == 2) {
            if (ch == '|') {
                current_field[idx] = '\0';
                
                // We found the ID *and* the PIN, thus return
                if (strcmp(current_field, input_pin) == 0) {
                    return 0; // SUCCESS
                } else {
                    return 1; // Wrong PIN
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
    
    // The ID was not found
    return -1;
    
    /*
    return 0 = success
    return -1 = invalid id
    return 1 = invalid pin
    */
}

int check_balance(int fd, const char *target_id, double* balance_output) {
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

	lseek(fd, 0, SEEK_SET);
    
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
	    } else if(ch == '\n' || ch == '\r') {
	    	idx = 0;
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
		state = 3;
	    }
	}

	// --- STATE 3: Read Balance ---
	else if (state == 3) {
	    if (ch == '\n' || ch == '\r') {
		current_field[idx] = '\0'; // Null-terminate Balance
		
		// Copy balance to output
		*balance_output = atof(current_field); 
		return 0; // SUCCESS
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

    return 1; // Account not found
}

int deposit_amount(int fd, int semid, const char *target_id, double* deposit_amt) {
    
	// Lock the file using p()
	p(semid);

	char ch;
	char current_field[MAX_FIELD];
	int idx = 0;
	off_t balance_position = 0;
	double old_balance, new_balance;

	// State machine
	// 0 = Reading AccountID
	// 1 = Skipping Username
	// 2 = Skipping PIN
	// 3 = Reading Balance
	// 4 = Skipping rest of line
	int state = 0;

	lseek(fd, 0, SEEK_SET);

	while (read(fd, &ch, 1) > 0) {

	// --- STATE 0: Read AccountID ---
	if (state == 0) {
	    if (ch == '|') {
		current_field[idx] = '\0';
		idx = 0;
		
		if (strcmp(current_field, target_id) == 0) {
		    state = 1;
		} else {
		    state = 4;
		}
	    } else if(ch == '\n' || ch == '\r') {
	    	idx = 0;
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
	    if (ch == '\n' || ch == '\r') {
		current_field[idx] = '\0';
		
		// Calculate new balance
		old_balance = atof(current_field);
		new_balance = old_balance + *deposit_amt;
		*deposit_amt = new_balance;
		
		// Convert new balance to string
		char new_balance_str[MAX_FIELD];
		int new_len = snprintf(new_balance_str, MAX_FIELD, "%.2f", new_balance);
		int old_len = strlen(current_field);
		
		// Seek back to balance position and write new balance
		lseek(fd, balance_position, SEEK_SET);
		write(fd, new_balance_str, new_len);
		
		// If new balance is shorter, pad with spaces
		for (int i = new_len; i < old_len; i++) {
		    write(fd, " ", 1);
		}
		
		// Unlock the file using v()
		v(semid);
		return 0; // Success
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

	

	v(semid);
	return 1;
}

int withdraw_amount(int fd, int semid, const char *target_id, double* withdraw_amt) {

    // Lock the file using p()
    p(semid);
    
    char ch;
    char current_field[MAX_FIELD];
    int idx = 0;
    off_t balance_position = 0;
    double current_balance, new_balance;
    
    
    // State machine
    // 0 = Reading AccountID
    // 1 = Skipping Username
    // 2 = Skipping PIN
    // 3 = Reading Balance
    // 4 = Skipping rest of line
    int state = 0;
    
    lseek(fd, 0, SEEK_SET);
    
    while (read(fd, &ch, 1) > 0) {
        
        // --- STATE 0: Read AccountID ---
        if (state == 0) {
            if (ch == '|') {
                current_field[idx] = '\0';
                idx = 0;
                
                if (strcmp(current_field, target_id) == 0) {
                    state = 1;
                } else {
                    state = 4;
                }
	    } else if(ch == '\n' || ch == '\r') {
	    	idx = 0;
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
            if (ch == '\n' || ch == '\r') {
                current_field[idx] = '\0';
                
                current_balance = atof(current_field);
                
                if (current_balance < *withdraw_amt) {
                    // Insufficient balance
                    v(semid);
                    return 1;
                }
                
                // Calculate new balance
                new_balance = current_balance - *withdraw_amt;
                *withdraw_amt = new_balance;
                
                // Convert new balance to string
                char new_balance_str[MAX_FIELD];
                int new_len = snprintf(new_balance_str, MAX_FIELD, "%.2f", new_balance);
                int old_len = strlen(current_field);
                
                // Seek back to balance position and write new balance
                lseek(fd, balance_position, SEEK_SET);
                write(fd, new_balance_str, new_len);
                
                // If new balance is shorter, pad with spaces
                for (int i = new_len; i < old_len; i++) {
                    write(fd, " ", 1);
                }
                
                v(semid);
                return 0; // Success
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
    
    v(semid);
    return 1; // Account not found
}

