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
#define ID_FIELD 8
#define USR_FIELD 20
#define PIN_FIELD 6
#define BAL_FIELD 15
#define ROW_SIZE 54

/* The offsets of fields in a row */
#define PIN_OFFSET (ID_FIELD + 1 + USR_FIELD + 1) // 8 + 1 + 20 + 1 = 30
#define BAL_OFFSET (PIN_OFFSET + PIN_FIELD + 1)    // 30 + 6 + 1 = 37

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

int search_row(int fd, const char* target_id) {
    char cur_id[ID_FIELD + 1];
    int row_index = 0;
    lseek(fd, 0, SEEK_SET);

    while(read(fd, cur_id, ID_FIELD) == ID_FIELD) {
        cur_id[ID_FIELD] = '\0'; // Null-terminate for strcmp
        
        /* If ID is found immediately return */
        if (strcmp(cur_id, target_id) == 0) return row_index;
        row_index++;
        
        /* Otherwise, jump to next row */
        if (lseek(fd, (off_t)row_index * ROW_SIZE, SEEK_SET) == (off_t)-1) break;
    }

    return -1; // If ID is not found
}

int verify_user(int fd, const char *target_id, const char *input_pin) {
	int row = search_row(fd, target_id);
	if (row == -1) return -1; // Return -1 immediately if ID is not found

	char stored_pin[PIN_FIELD + 1];
	/* Jump to the PIN field of the row with ID */
	lseek(fd, (off_t)row * ROW_SIZE + PIN_OFFSET, SEEK_SET);

	if (read(fd, stored_pin, PIN_FIELD) != PIN_FIELD) return -1;
	stored_pin[PIN_FIELD] = '\0';
	
	/* Return 0 if PIN is valid, 1 if invalid */
	return (strcmp(stored_pin, input_pin) == 0) ? 0 : 1;
	
	/*
	return 0 = success
	return -1 = invalid id
	return 1 = invalid pin
	*/
}

int check_balance(int fd, const char *target_id, double* balance_output) {
    int row = search_row(fd, target_id);
    if (row == -1) return -1;

    char bal_str[BAL_FIELD + 1];
    /* Jump to the balance field of the row */
    lseek(fd, (off_t)row * ROW_SIZE + BAL_OFFSET, SEEK_SET);
    
    if (read(fd, bal_str, BAL_FIELD) != BAL_FIELD) return 1;
    bal_str[BAL_FIELD] = '\0';

    *balance_output = atof(bal_str);
    return 0;
}

int deposit_amount(int fd, int semid, const char *target_id, double* deposit_amt) {
    p(semid);
    int row = search_row(fd, target_id);
    if (row == -1) {
    	v(semid); 
    	return -1; /* If row not found, unlock semaphore and return -1 */
    }

    char bal_str[BAL_FIELD + 1];
    off_t bal_pos = (off_t)row * ROW_SIZE + BAL_OFFSET;
    lseek(fd, bal_pos, SEEK_SET);
    
    if (read(fd, bal_str, BAL_FIELD) != BAL_FIELD) {
    	v(semid); 
    	return -1;
    }
    
    bal_str[BAL_FIELD] = '\0';

    /* Update back to server the new_bal */
    double new_balance = atof(bal_str) + *deposit_amt;
    *deposit_amt = new_balance;

    /* Overwrite the balance field with new amount */
    snprintf(bal_str, BAL_FIELD + 1, "%015.2f", new_balance);
    lseek(fd, bal_pos, SEEK_SET);
    write(fd, bal_str, BAL_FIELD);

    v(semid);
    return 0;
}

int withdraw_amount(int fd, int semid, const char *target_id, double* withdraw_amt) {
    p(semid);
    int row = search_row(fd, target_id);
    if (row == -1) {
    	v(semid); 
    	return -1; /* If row not found, unlock semaphore and return -1 */
    }

    char bal_str[BAL_FIELD + 1];
    off_t bal_pos = (off_t)row * ROW_SIZE + BAL_OFFSET;
    lseek(fd, bal_pos, SEEK_SET);
    
    if (read(fd, bal_str, BAL_FIELD) != BAL_FIELD) {
    	v(semid); 
    	return -1;
    }
    
    bal_str[BAL_FIELD] = '\0';

    /* Checks if current balance is enough to withdraw */
    double current_balance = atof(bal_str);
    if (current_balance < *withdraw_amt) {
    	v(semid); 
    	return -1;
    }

    /* Update back to server the new_bal */
    double new_balance = current_balance - *withdraw_amt;
    *withdraw_amt = new_balance;

    /* Overwrite the balance field with new amount */
    snprintf(bal_str, BAL_FIELD + 1, "%015.2f", new_balance);
    lseek(fd, bal_pos, SEEK_SET);
    write(fd, bal_str, BAL_FIELD);

    v(semid);
    return 0;
}

