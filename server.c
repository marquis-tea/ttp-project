/* server.c */
#include "common.h"
#include "database.h"

int server_login(int, int, char*, char*);
int server_checkbal(int, int, char*, char*);
int server_withdraw(int, int, int, char*, char*);
int server_deposit(int, int, int, char*, char*);
int alert_error(char*, int);
int update_log(char*, char*, char*, int);

int main() {
	int sockfd, cli_len, nready, max_i, nrecv, fifo_fd, msgid, acc_fd, semid;
	struct pollfd pollfds[MAX_CLI];
	char pollid[MAX_CLI][ID_LEN + 1];
	char buf[MAX_BUF], rsp[MAX_BUF];
	struct sockaddr_in serv_addr, cli_addr;
	
	/* Setting up the server-side socket */
	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	serv_addr.sin_addr.s_addr = inet_addr("192.168.0.16"); /* Change server IP address here */
	
	if(fork() == 0) {
		printf("Starting logger module...\n");
		execl("./logger", "logger", (char *) 0);
		perror("execl() failed");
		exit(1);
	}
	sleep(1);
	
	printf("Creating socket...\n");
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
	
	printf("Binding socket...\n");
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
	else printf("Created semaphore...\n");
	
	// Initialize semaphore to 1 (unlocked)
	union semun arg;
	arg.val = 1;
	semctl(semid, 0, SETVAL, arg);
    
	/* Connecting to FIFO pipe */
	if((fifo_fd = open(FIFO_NAME, O_WRONLY | O_NONBLOCK)) < 0) perror("Open FIFO failed");
	else printf("Connected to FIFO pipe...\n");
	
	/* Connecting to Message Queue */
	if((msgid = msgget(MSG_KEY, 0666)) < 0) perror("Get message queue failed");
	else printf("Connected to message queue...\n");
	
	printf("Listening for connection request...\n");
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
					
					if(strncmp(buf, LGOUT, strlen(LGOUT)) == 0) {
						printf("Client requested logout.\n");
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
					
					printf("\nReceive message %s from client %s.\n", buf, inet_ntoa(cli_addr.sin_addr));
					update_log(pollid[i], buf, rsp, fifo_fd);
					break;
				}
				
				if(--nready <= 0) break;
			}
		}
	}
	close(sockfd);
	close(fifo_fd);
	close(acc_fd);
}

/* Server Operations */

int update_log(char* sid, char* cmd, char* rsp, int fifo_fd) {
	time_t now = time(NULL);
	char* timestamp = ctime(&now);
	char buf[MAX_BUF];
	
	timestamp[strcspn(timestamp, "\n")] = 0;
	
	snprintf(buf, MAX_BUF, "[%s]|%s|%s|%s", timestamp, sid, cmd, rsp);
	printf("Log entry: %s\n", buf);
	
	if(write(fifo_fd, buf, strlen(buf) + 1) < 0) perror("Write FIFO failed");
	
	return(0);
}

int alert_error(char* sid, int msgid) {
	struct q_entry alert_msg;
	int id = atoi(sid);
	
	printf("Sending alert!\n");
	alert_msg.mtype = (long) id;
	snprintf(alert_msg.mtext, MAX_ALRT, "[ALERT!!!] wrong PIN entered for ID = %s", sid);
	
	if(msgsnd(msgid, &alert_msg, strlen(alert_msg.mtext) + 1, 0) < 0) perror("msgsnd() failed.");
	
	return(0);
}

int server_login(int sockfd, int acc_fd, char* cmd, char* rsp) {
	int result, valid_id = 0, valid_pin = 0;
	
	/* Parse the ID and PIN from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");
	char *pin = strtok(NULL, ":");

	result = verify_user(acc_fd, id, pin);
	if(result < 0) {
		valid_id = 0;
	} else if(result > 0) {
		valid_id = 1;
		valid_pin = 0;
	} else {
		valid_id = 1;
		valid_pin = 1;
	}
	
	if(!valid_id) {
		if(send(sockfd, ERR_ID, strlen(ERR_ID), 0) < 0) perror("Send failed");
		strcpy(rsp, ERR_ID);
		return(1);
	} else if(!valid_pin) {
		if(send(sockfd, ERR_PIN, strlen(ERR_PIN), 0) < 0) perror("Send failed");
		strcpy(rsp, ERR_PIN);
		return(-1);
	}
	
	if(send(sockfd, OK, strlen(OK) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, OK);
	return(0);
}

int server_checkbal(int sockfd, int acc_fd, char* cmd, char* rsp) {
	double bal = 0;
	char buf[MAX_BUF];
	
	/* Parse the ID from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");

	check_balance(acc_fd, id, &bal);
	
	snprintf(buf, MAX_BUF, "%s:%.2f", VAL, bal); /* Concatenate VAL:balance */
	printf("%s\n", buf);
	
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, buf);
	return(0);
}

int server_withdraw(int sockfd, int acc_fd, int semid, char* cmd, char* rsp) {
	double bal = 0;
	int insuff_bal = 0;
	char buf[MAX_BUF];
	
	/* Parse the ID and Amount from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");
	char *amt = strtok(NULL, ":");
	
	bal = atof(amt);

	insuff_bal = withdraw_amount(acc_fd, semid, id, &bal);
	
	if(insuff_bal) {
		if(send(sockfd, ERR_BAL, strlen(ERR_BAL), 0) < 0) perror("Send failed");
		strcpy(rsp, ERR_BAL);
		return(-1);
	}
	
	snprintf(buf, MAX_BUF, "%s:%.2f", OK, bal); /* Concatenate OK:new_bal */
	
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, buf);
	return(0);
}

int server_deposit(int sockfd, int acc_fd, int semid, char* cmd, char* rsp) {
	double bal = 0;
	char buf[MAX_BUF];
	
	/* Parse the ID and Amount from the client command */
	char *tmp = strtok(cmd, ":");
	char *id = strtok(NULL, ":");
	char *amt = strtok(NULL, ":");
	
	bal = atof(amt);

	deposit_amount(acc_fd, semid, id, &bal);
	
	snprintf(buf, MAX_BUF, "%s:%.2f", OK, bal); /* Concatenate OK:new_bal */
	
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("Send failed");
	strcpy(rsp, buf);
	return(0);
}


