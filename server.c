/* server.c */
#include "common.h"

int server_login(int, char*);
int server_checkbal(int, char*);
int server_withdraw(int, char*);
int server_deposit(int, char*);
int alert_error(char*);
int update_log(char*, char*, char*);

int main() {
	int sockfd, cli_len, nready, max_i, nrecv;
	struct pollfd pollfds[MAX_CLI];
	char buf[MAX_BUF];
	struct sockaddr_in serv_addr, cli_addr;

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); /* Change server IP address here */
	
	if(fork() == 0) {
		printf("Starting logger module...\n");
		execl("./logger", "logger", (char *) 0);
		perror("!execl() failed!");
		exit(1);
	}
	
	printf("Creating socket...\n");
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("!socket() error!");
		exit(1);
	}
	
	printf("Binding socket...\n");
	if((bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
		perror("!bind() error!");
		exit(1);
	}

	printf("Listening for connection request...\n");
	listen(sockfd, 5);
	
	pollfds[0].fd = sockfd; /* Set sockfd as the listening fd for accepting new clients */
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
				perror("!Too many clients!");
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
					case -1:
					if(errno == ECONNRESET) {
						close(curfd);
						pollfds[i].fd = -1;
					} else perror("!Read error!");
					break;
						
					case 0:
					close(curfd);
					pollfds[i].fd = -1;
					break;
					
					default: 
					/* Client data handling here */
					if(nrecv < MAX_BUF) buf[nrecv] = '\0';
					
					if(strncmp(buf, "LGOUT", 5) == 0) {
						printf("Client requested logout.\n");
						close(curfd);
						pollfds[i].fd = -1;
						break;
					} else if(strncmp(buf, LGN, strlen(LGN)) == 0) server_login(curfd, buf);
					else if(strncmp(buf, BAL, strlen(BAL)) == 0) server_checkbal(curfd, buf);
					else if(strncmp(buf, WDW, strlen(WDW)) == 0) server_withdraw(curfd, buf);
					else if(strncmp(buf, DEP, strlen(DEP)) == 0) server_deposit(curfd, buf);
					
					printf("Receive message %s from client %s.\n", buf, inet_ntoa(cli_addr.sin_addr));
					break;
				}
				
				if(--nready <= 0) break;
			}
		}
	}
	close(sockfd);
}

int update_log(char* sid, char* cmd, char* rsp) {
	time_t now = time(NULL);
	char* timestamp = ctime(&now);
	char buf[MAX_BUF];
	
	timestamp[strcspn(timestamp, "\n")] = 0;
	
	snprintf(buf, MAX_BUF, "[%s]|%s|%s|%s", timestamp, sid, cmd, rsp);
	printf("Log entry: %s\n", buf);
	
	/* Logger operation here */
	
	return(0);
}

int alert_error(char* sid) {
	struct q_entry alert_msg;
	int id = atoi(sid);
	char* msg = "[ALERT!!!] wrong PIN entered.";
	
	alert_msg.mtype = (long) id;
	strncpy(alert_msg.mtext, msg, strlen(msg) + 1);
	
	/* Logger operation here */
	
	return(0);
}

int server_login(int sockfd, char* cmd) {
	char* sid = "1111"; // Change this later
	int valid_id = 0, valid_pin = 0;
	
	/* Database control here */
	
	if(!valid_id) {
		if(send(sockfd, ERR_ID, strlen(ERR_ID) + 1, 0) < 0) perror("!Send failed!");
		update_log(sid, cmd, ERR_ID);
		return(-1);
	} else if(!valid_pin) {
		if(send(sockfd, ERR_PIN, strlen(ERR_PIN) + 1, 0) < 0) perror("!Send failed!");
		update_log(sid, cmd, ERR_PIN);
		alert_error(sid);
		return(-1);
	} 
	
	if(send(sockfd, OK, strlen(OK) + 1, 0) < 0) perror("!Send failed!");
	update_log(sid, cmd, OK);
	return(0);
}

int server_checkbal(int sockfd, char* cmd) {
	char* sid = "1111"; // Change this later
	int balance = 100; // Change this later
	char buf[MAX_BUF];
	
	/* Database control here */
	
	snprintf(buf, MAX_BUF, "%s:%d", VAL, balance); /* Concatenate VAL:balance */
	
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("!Send failed!");
	update_log(sid, cmd, buf);
	return(0);
}

int server_withdraw(int sockfd, char* cmd) {
	char* sid = "1111"; // Change this later
	int balance = 100; // Change this later
	int suff_bal = 0;
	char buf[MAX_BUF];
	
	/* Database control here */
	
	if(!suff_bal) {
		if(send(sockfd, ERR_BAL, strlen(ERR_BAL) + 1, 0) < 0) perror("!Send failed!");
		update_log(sid, cmd, ERR_BAL);
		return(-1);
	}
	
	snprintf(buf, MAX_BUF, "%s:%d", OK, balance); /* Concatenate OK:new_bal */
	
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("!Send failed!");
	update_log(sid, cmd, buf);
	return(0);
}

int server_deposit(int sockfd, char* cmd) {
	char* sid = "1111"; // Change this later
	int balance = 100; // Change this later
	char buf[MAX_BUF];
	
	/* Database control here */
	
	snprintf(buf, MAX_BUF, "%s:%d", OK, balance); /* Concatenate OK:new_bal */
	
	if(send(sockfd, buf, strlen(buf) + 1, 0) < 0) perror("!Send failed!");
	update_log(sid, cmd, buf);
	return(0);
}
