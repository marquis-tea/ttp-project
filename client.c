#include "common.h"

// ANSI Color Codes
#define BOLD_BLUE  "\033[1;34m"
#define BOLD_CYAN  "\033[1;36m"
#define BOLD_WHITE "\033[1;37m"
#define RESET      "\033[0m"
#define BOLD_RED   "\033[1;31m"
#define BOLD_GREEN "\033[1;32m"

int sockfd = 0;

/* Clears input buffer */
void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* Loading animation */
void show_loading(const char *msg) {
    printf("%s%s", BOLD_CYAN, msg);
    for (int i = 0; i < 3; i++) {
        printf(".");
        fflush(stdout);
        usleep(300000); 
    }
    printf("%s\n", RESET);
}

/* Handling inactive user */
void handle_inactivity(int sig) {
    printf("\n\n%s[TIMEOUT]%s Session expired. Logging out...\n", BOLD_RED, RESET);
    if (sockfd > 0) {
        if(send(sockfd, LGOUT, strlen(LGOUT) + 1, 0) < 0) perror("!Send failed!");
        close(sockfd);
    }
    exit(0);
}

int main() {
	char temp_id[64], temp_pin[64];
	char id[ID_LEN + 1], pin[PIN_LEN + 1];
	char send_buf[MAX_BUF], recv_buf[MAX_BUF];
	struct sockaddr_in serv_addr;
	int auth = 0;
	
	/* Setup alarm signal handler */
	static struct sigaction act;
	act.sa_handler = handle_inactivity;
	sigaction(SIGALRM, &act, (void *) 0);
    	
    	/* Connect to the server */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero((char *)&serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	serv_addr.sin_addr.s_addr = inet_addr("192.168.0.16");

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("%s[!] SYSTEM OFFLINE. Try again later.%s\n", BOLD_RED, RESET);
		close(sockfd);
		exit(1);
	}

	// --- MAIN LOGIN LOOP ---
	while (!auth) {
		system("clear");
		printf("%s====================================================%s\n", BOLD_BLUE, RESET);
		printf("%s           WELCOME TO THE SIXSEVEN BANK ATM         %s\n", BOLD_WHITE, RESET);
		printf("%s====================================================%s\n\n", BOLD_BLUE, RESET);
		
		/* Validate ID input */
		printf("%s» ACCOUNT ID: %s", BOLD_CYAN, RESET);
		if (scanf("%63s", temp_id) <= 0) exit(0);
		clear_stdin();
		if (strlen(temp_id) != ID_LEN) {
		    printf("%s[X] ERROR: INVALID ID . TRY AGAIN...%s\n", BOLD_RED, RESET);
		    sleep(1); continue; 
		}
		strcpy(id, temp_id);
		
		/* Validate PIN input */
		printf("%s» ACCESS PIN: %s", BOLD_CYAN, RESET);
		if (scanf("%63s", temp_pin) <= 0) exit(0);
		clear_stdin();
		if (strlen(temp_pin) != PIN_LEN) {
		    printf("%s[X] ERROR: INVALID PIN . TRY AGAIN...%s\n", BOLD_RED, RESET);
		    sleep(1); continue; 
		}
		strcpy(pin, temp_pin);
		
		/* Send LGN to client to validate ID and PIN */
		snprintf(send_buf, MAX_BUF, LGN ":%s:%s", id, pin);
		send(sockfd, send_buf, strlen(send_buf) + 1, 0);
		
		show_loading("\nVerifying credentials");

		bzero(recv_buf, sizeof(recv_buf));
		recv(sockfd, recv_buf, sizeof(recv_buf), 0);

		if (strcmp(recv_buf, OK) == 0) {
		    auth = 1; 
		    printf("%s[✔] Identity Verified!%s\n", BOLD_GREEN, RESET);
		    sleep(1);
		} else {
		    printf("%s[X] LOGIN FAILED: %s. RESTARTING...%s\n", BOLD_RED, recv_buf, RESET);
		    close(sockfd);
		    sleep(2); 
		}
	}
	
	alarm(TIMEOUT);

	int choice;
	float amount;
	for(;;) {
		system("clear");
		printf("%s====================================================%s\n", BOLD_BLUE, RESET);
		printf("%s                  MAIN TRANSACTION MENU             %s\n", BOLD_WHITE, RESET);
		printf("%s====================================================%s\n", BOLD_BLUE, RESET);
		printf("%s 1.%s Check Balance\n%s 2.%s Deposit Funds\n%s 3.%s Withdraw Cash\n%s 4.%s Logout\n", 
		       BOLD_CYAN, RESET, BOLD_CYAN, RESET, BOLD_CYAN, RESET, BOLD_CYAN, RESET);
		printf("----------------------------------------------------\nSelection: ");

		if (scanf("%d", &choice) <= 0) break;
		clear_stdin();
		alarm(TIMEOUT);

		if (choice == 1) { // --- CHECK BALANCE ---
			snprintf(send_buf, MAX_BUF, BAL ":%s", id);
			send(sockfd, send_buf, strlen(send_buf) + 1, 0);
			
			bzero(recv_buf, sizeof(recv_buf));
			recv(sockfd, recv_buf, sizeof(recv_buf), 0);
			printf("Received balance.\n");
			
			char* bal = strchr(recv_buf, ':');
			bal++; /* Parse message to get the balance */
			
			printf("\n%s>> BALANCE: RM%s <<%s\n", BOLD_GREEN, bal, RESET);
		} 
		else if (choice == 2) { // --- DEPOSIT ---
			printf("\n%sEnter DEPOSIT amount: RM%s", BOLD_CYAN, RESET);
			scanf("%f", &amount);
			clear_stdin();

			show_loading("Processing Deposit");
			snprintf(send_buf, MAX_BUF, DEP ":%s:%.2f", id, amount); 
			send(sockfd, send_buf, strlen(send_buf) + 1, 0);

			bzero(recv_buf, sizeof(recv_buf));
			recv(sockfd, recv_buf, sizeof(recv_buf), 0);
			
			char* bal = strchr(recv_buf, ':');
			bal++; /* Parse message to get the balance */
			
			printf("%s[✔] %s%s\n", BOLD_GREEN, bal, RESET);
		}
		else if (choice == 3) { // --- WITHDRAWAL ---
			printf("\n%sEnter WITHDRAWAL amount: RM%s", BOLD_CYAN, RESET);
			scanf("%f", &amount);
			clear_stdin();

			show_loading("Processing Withdrawal");
			snprintf(send_buf, MAX_BUF, WDW ":%s:%.2f", id, amount);
			send(sockfd, send_buf, strlen(send_buf) + 1, 0);

			bzero(recv_buf, sizeof(recv_buf));
			recv(sockfd, recv_buf, sizeof(recv_buf), 0);

			if (strncmp(recv_buf, OK, strlen(OK)) == 0) {
				char* bal = strchr(recv_buf, ':');
				bal++; /* Parse message to get the balance */
				
				printf("%s[✔] %s%s\n", BOLD_GREEN, bal, RESET);
			} else {
				printf("%s[X] ERROR: %s%s\n", BOLD_RED, recv_buf, RESET);
			}
		}
		else if (choice == 4) { // --- LOGOUT ---
			if(send(sockfd, LGOUT, strlen(LGOUT) + 1, 0) < 0) perror("!Send failed!");
			printf("\n%sLogging out... THANK YOU!!%s\n", BOLD_CYAN, RESET);
			sleep(1);
			break;
		} else {
			printf("\n%s[X] ERROR: INVALID INPUT . TRY AGAIN... %s\n", BOLD_RED, RESET);
			sleep(1);
		}
		printf("\nPress Enter to return...");
		getchar();
	}

	close(sockfd);
	return 0;
}
