/* server.c */
#include "common.h"
#include "database.h"

int sockfd, fifo_fd, acc_fd, msgid, semid, log_pid;

/* Function prototypes */
int server_login(int, int, char*, char*);
int server_checkbal(int, int, char*, char*);
int server_withdraw(int, int, int, char*, char*);
int server_deposit(int, int, int, char*, char*);
int alert_error(char*, int);
int update_log(char*, char*, char*, int);
void handle_shutdown(int);

int main() {
	int cli_len, nready, max_i, nrecv;
	struct pollfd pollfds[MAX_CLI];
	char pollid[MAX_CLI][ID_LEN + 1];
	char buf[MAX_BUF], rsp[MAX_BUF];
	struct sockaddr_in serv_addr, cli_addr;
	
	/* Setting up the server-side socket */
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	serv_addr.sin_addr.s_addr = inet_addr("192.168.0.16"); /* Change server IP address here */
	
	printf("[SERVER] Creating socket...\n");
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket() error");
		exit(1);
	}
	
	/* Enable reusing address for the socket */
	int opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt() failed");
		close(sockfd);
		exit(1);
	}
	
	/* Binding the socket */
	printf("[SERVER] Binding socket...\n");
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("bind() error");
		close(sockfd);
		exit(1);
	}
	
	/* Opening accounts.txt */
	acc_fd = open(ACC_FILE, O_RDWR);
	if (acc_fd == -1) {
		perror("Error opening file");
		return 1;
    	}	
		
    	/* Creating Semaphore */
	semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
	if (semid == -1) perror("Error creating semaphore");
	else printf("[SERVER] Created semaphore...\n");
	
	/* Initialize semaphore to 1 (unlocked) */
	union semun arg;
	arg.val = 1;
	semctl(semid, 0, SETVAL, arg);
    
	/* Creating FIFO pipe */
	if (mkfifo(FIFO_NAME, 0666) < 0) {
		if (errno != EEXIST) {
			perror("mkfifo() failed");
			exit(1);
		} else printf("Created FIFO pipe...\n");
	}
	
	/* Connecting to/Creating Message Queue */
	if((msgid = msgget(MSG_KEY, 0666 | IPC_CREAT)) < 0) perror("Get message queue failed");
	else printf("[SERVER] Connected to message queue...\n");
	
	/* Forking and executing logger program */
	if((log_pid = fork()) == 0) {
		printf("[SERVER] Starting logger module...\n");
		execl("./logger", "logger", (char *) 0);
		perror("execl() failed");
		exit(1);
	}
	
	/* Opening the FIFO pipe */
	if((fifo_fd = open(FIFO_NAME, O_WRONLY)) < 0) perror("Open FIFO failed");
	else printf("[SERVER] Connected to FIFO pipe...\n");
	
	/* Setup shutdown signal handler */
	static struct sigaction act;
	act.sa_handler = handle_shutdown;
	sigaction(SIGINT, &act, (void *) 0);
	
	printf("[SERVER] Listening for connection request...\n");
	listen(sockfd, 5);
	
	/* Set sockfd as the listening fd for accepting new clients */
	pollfds[0].fd = sockfd; 
	pollfds[0].events = POLLRDNORM;
	
	for(int i = 1; i < MAX_CLI; i++) {
		pollfds[i].fd = -1; /* -1 indicates available entry */
	}
	
	max_i = 0;
	
	for(;;) {
		nready = poll(pollfds, max_i + 1, -1); /* Monitor the pollfds for events to happen such as data to be read */
		
		if(pollfds[0].revents & POLLRDNORM) { /* When a client send connection request to sockfd */
			cli_len = sizeof(cli_addr);
			int new_sockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &cli_len); /* Accept and create a new sockfd */
			printf("\nClient %s is now connected.\n", inet_ntoa(cli_addr.sin_addr));
			
			int i;
			for(i = 1; i < MAX_CLI; i++) {
				if(pollfds[i].fd < 0) {
					pollfds[i].fd = new_sockfd;  /* Save the fd into pollfds */
					pollfds[i].events = POLLRDNORM;
					break;
				}
			}
				
			if(i == MAX_CLI) {
				perror("Too many clients");
				exit(1);
			}
			
			if(i > max_i) max_i = i; /* Update max index */
			if(--nready <= 0) continue; 
		}
		
		for(int i = 1; i <= max_i; i++) {
			int curfd = pollfds[i].fd;
			if(curfd < 0) continue;
			
			if(pollfds[i].revents & (POLLRDNORM | POLLERR) ) {
				switch(nrecv = recv(curfd, buf, MAX_BUF, 0)) {
					
					/* When error received from recv() */
					case -1:
					if(errno == ECONNRESET) {
						close(curfd);
						pollfds[i].fd = -1;
					} else perror("Read error");
					break;
					
					/* If client don't send any data */
					case 0:
					
					close(curfd);
					pollfds[i].fd = -1;
					break;
					
					
					/* Client data handling here */
					default: 
					
					if(nrecv < MAX_BUF) buf[nrecv] = '\0';
					printf("\nReceived message %s from client %s.\n", buf, inet_ntoa(cli_addr.sin_addr));
					
					if(strncmp(buf, LGOUT, strlen(LGOUT)) == 0) {
						printf("\nClient %s requested logout.\n", inet_ntoa(cli_addr.sin_addr));
						close(curfd);
						pollfds[i].fd = -1;
						break;
					} else if(strncmp(buf, LGN, strlen(LGN)) == 0) {
						char *incoming_id = strchr(buf, ':') + 1;
	            
						// Copy the account id into the pollid[][] at the current index 'i'
						strncpy(pollid[i], incoming_id, ID_LEN);
						pollid[i][ID_LEN] = '\0'; // Ensure termination
					
						if(server_login(curfd, acc_fd, buf, rsp) < 0) {
							alert_error(pollid[i], msgid);
						}
					}
					else if(strncmp(buf, BAL, strlen(BAL)) == 0)
						server_checkbal(curfd, acc_fd, buf, rsp);
						
					else if(strncmp(buf, WDW, strlen(WDW)) == 0)
						server_withdraw(curfd, acc_fd, semid, buf, rsp);
						
					else if(strncmp(buf, DEP, strlen(DEP)) == 0)
						server_deposit(curfd, acc_fd, semid, buf, rsp);
					
					update_log(pollid[i], buf, rsp, fifo_fd);
					break;
				}
				
				if(--nready <= 0) break;
			}
		}
	}
}

/* Handling Ctrl+C to shutdown server */
void handle_shutdown(int sig) {
	printf("\n\n[SERVER] Caught Ctrl+C! Cleaning up...\n");

	// Killing the logger process
	if (log_pid > 0) {
		printf("[SERVER] Stopping logger module (PID %d)...\n", log_pid);
		kill(log_pid, SIGTERM);
		waitpid(log_pid, NULL, 0); // Wait for it to die
	}

	// Removing message queues
	if (msgid != -1) {
		msgctl(msgid, IPC_RMID, NULL);
		printf("[SERVER] Removed Message Queue.\n");
	}

	// Removing semaphores
	if (semid != -1) {
		semctl(semid, 0, IPC_RMID);
		printf("[SERVER] Removed Semaphore.\n");
	}

	// Closing all files
	close(sockfd);
	close(fifo_fd);
	close(acc_fd);

	// Deleting the FIFO file
	unlink(FIFO_NAME);

	printf("[SERVER] Shutdown complete. Goodbye.\n");
	exit(0);
}

/* Server Operations */

/* Updating client command and server resonse to log */
int update_log(char* sid, char* cmd, char* rsp, int fifo_fd) {
	time_t now = time(NULL);
	char* timestamp = ctime(&now);
	char buf[MAX_BUF];
	
	timestamp[strcspn(timestamp, "\n")] = 0;
	
	snprintf(buf, MAX_BUF, "[%s]|%s|%s|%s", timestamp, sid, cmd, rsp);
	
	if(write(fifo_fd, buf, strlen(buf)) < 0) perror("Write FIFO failed");
	
	return(0);
}

/* Posting alert to message queue */
int alert_error(char* sid, int msgid) {
	struct q_entry alert_msg;
	int id = atoi(sid);
	
	printf("Sending alert!\n");
	alert_msg.mtype = (long) id;
	snprintf(alert_msg.mtext, MAX_ALRT, "[ALERT!!!] wrong PIN entered for ID = %s", sid);
	
	if(msgsnd(msgid, &alert_msg, strlen(alert_msg.mtext), 0) < 0) perror("msgsnd() failed.");
	
	return(0);
}

/* Verifying ID and PIN with database */
int server_login(int sockfd, int acc_fd, char* cmd, char* rsp) {
	int result, valid_id = 0, valid_pin = 0;
	
	/* Parse the ID and PIN from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");
	char *pin = strtok(NULL, ":");

	result = verify_user(acc_fd, id, pin);
	if(result > 0) {
		valid_id = 1;
	} else {
		valid_id = 1;
		valid_pin = 1;
	}
	
	if(!valid_id) {
		printf("[SERVER] Sending to client: %s\n", ERR_ID);
		if(send(sockfd, ERR_ID, strlen(ERR_ID), 0) < 0) perror("Send failed");
		strcpy(rsp, ERR_ID);
		return(1);
	} else if(!valid_pin) {
		printf("[SERVER] Sending to client: %s\n", ERR_PIN);
		if(send(sockfd, ERR_PIN, strlen(ERR_PIN), 0) < 0) perror("Send failed");
		strcpy(rsp, ERR_PIN);
		return(-1);
	}
	
	printf("[SERVER] Sending to client: %s\n", OK);
	if(send(sockfd, OK, strlen(OK) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, OK);
	return(0);
}

/* Handling check balance command */
int server_checkbal(int sockfd, int acc_fd, char* cmd, char* rsp) {
	double bal = 0;
	char buf[MAX_BUF];
	
	/* Parse the ID from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");

	if(check_balance(acc_fd, id, &bal) < 0) perror("Check balance failed");
	
	snprintf(buf, MAX_BUF, "%s:%.2f", VAL, bal); /* Concatenate VAL:balance */
	
	printf("[SERVER] Sending to client: %s\n", buf);
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, buf);
	return(0);
}

/* Handling withdrawal command */
int server_withdraw(int sockfd, int acc_fd, int semid, char* cmd, char* rsp) {
	double bal = 0;
	int status = 0;
	char buf[MAX_BUF];
	
	/* Parse the ID and Amount from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");
	char *amt = strtok(NULL, ":");
	
	bal = atof(amt);

	status = withdraw_amount(acc_fd, semid, id, &bal);
	
	if(status < 0) {
		printf("[SERVER] Sending to client: %s\n", ERR_BAL);
		if(send(sockfd, ERR_BAL, strlen(ERR_BAL), 0) < 0) perror("Send failed");
		strcpy(rsp, ERR_BAL);
		return(-1);
	}
	
	snprintf(buf, MAX_BUF, "%s:%.2f", OK, bal); /* Concatenate OK:new_bal */
	
	printf("[SERVER] Sending to client: %s\n", buf);
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, buf);
	return(0);
}

/* Handling deposit command */
int server_deposit(int sockfd, int acc_fd, int semid, char* cmd, char* rsp) {
	double bal = 0;
	char buf[MAX_BUF];
	
	/* Parse the ID and Amount from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");
	char *amt = strtok(NULL, ":");
	
	bal = atof(amt);

	if(deposit_amount(acc_fd, semid, id, &bal) < 0) perror("Deposit amount failed");
	
	snprintf(buf, MAX_BUF, "%s:%.2f", OK, bal); /* Concatenate OK:new_bal */
	
	printf("[SERVER] Sending to client: %s\n", buf);
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, buf);
	return(0);
}


