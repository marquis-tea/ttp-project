#include "common.h"

// ANSI Color Codes
#define BOLD_BLUE  "\033[1;34m"
#define BOLD_CYAN  "\033[1;36m"
#define BOLD_WHITE "\033[1;37m"
#define RESET      "\033[0m"
#define BOLD_RED   "\033[1;31m"
#define BOLD_GREEN "\033[1;32m"

int sock_fd = 0;

void clear_stdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void show_loading(const char *msg) {
    printf("%s%s", BOLD_CYAN, msg);
    for (int i = 0; i < 3; i++) {
        printf(".");
        fflush(stdout);
        usleep(300000); 
    }
    printf("%s\n", RESET);
}

void handle_inactivity(int sig) {
    printf("\n\n%s[TIMEOUT]%s Session expired. Logging out...\n", BOLD_RED, RESET);
    if (sock_fd > 0) {
        send(sock_fd, "LGOUT", 5, 0);
        close(sock_fd);
    }
    exit(0);
}

int main() {
    char temp_id[64], temp_pin[64];
    char final_id[9], final_pin[7];
    char send_buf[1024], recv_buf[1024];
    struct sockaddr_in serv_addr;
    int authenticated = 0;

    // --- MAIN LOGIN LOOP ---
    while (!authenticated) {
        system("clear");
        printf("%s====================================================%s\n", BOLD_BLUE, RESET);
        printf("%s           WELCOME TO THE SIXSEVEN BANK ATM         %s\n", BOLD_WHITE, RESET);
        printf("%s====================================================%s\n\n", BOLD_BLUE, RESET);

        printf("%s» ACCOUNT ID: %s", BOLD_CYAN, RESET);
        if (scanf("%63s", temp_id) <= 0) exit(0);
        clear_stdin();
        if (strlen(temp_id) != 8) {
            printf("%s[X] ERROR: INVALID ID . TRY AGAIN...%s\n", BOLD_RED, RESET);
            sleep(1); continue; 
        }
        strcpy(final_id, temp_id);

        printf("%s» ACCESS PIN: %s", BOLD_CYAN, RESET);
        if (scanf("%63s", temp_pin) <= 0) exit(0);
        clear_stdin();
        if (strlen(temp_pin) != 6) {
            printf("%s[X] ERROR: INVALID PIN . TRY AGAIN...%s\n", BOLD_RED, RESET);
            sleep(1); continue; 
        }
        strcpy(final_pin, temp_pin);

        show_loading("\nVerifying credentials");
        
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        
        // --- CHANGED TO BZERO ---
        bzero((char *)&serv_addr, sizeof(serv_addr));
        
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERV_TCP_PORT);
        serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("%s[!] SYSTEM OFFLINE. Try again later.%s\n", BOLD_RED, RESET);
            close(sock_fd);
            sleep(2);
            continue; 
        }

        sprintf(send_buf, "LGN:%s:%s", final_id, final_pin);
        send(sock_fd, send_buf, strlen(send_buf), 0);

        // --- CHANGED TO BZERO ---
        bzero(recv_buf, sizeof(recv_buf));
        recv(sock_fd, recv_buf, sizeof(recv_buf), 0);

        if (strcmp(recv_buf, "OK") == 0) {
            authenticated = 1; 
            printf("%s[✔] Identity Verified!%s\n", BOLD_GREEN, RESET);
            sleep(1);
        } else {
            printf("%s[X] LOGIN FAILED: %s. RESTARTING...%s\n", BOLD_RED, recv_buf, RESET);
            close(sock_fd);
            sleep(2); 
        }
    }

    signal(SIGALRM, handle_inactivity);
    alarm(TIMEOUT);

    int choice;
    float amount;
    while (1) {
        system("clear");
        printf("%s====================================================%s\n", BOLD_BLUE, RESET);
        printf("%s                  MAIN TRANSACTION MENU             %s\n", BOLD_WHITE, RESET);
        printf("%s====================================================%s\n", BOLD_BLUE, RESET);
        printf("%s 1.%s Check Balance\n%s 2.%s Deposit Funds\n%s 3.%s Withdraw Cash\n%s 4.%s Secure Logout\n", 
               BOLD_CYAN, RESET, BOLD_CYAN, RESET, BOLD_CYAN, RESET, BOLD_CYAN, RESET);
        printf("----------------------------------------------------\nSelection: ");
        
        if (scanf("%d", &choice) <= 0) break;
        clear_stdin();
        alarm(TIMEOUT); 

        if (choice == 1) { // --- CHECK BALANCE ---
            send(sock_fd, "BAL", 3, 0);
            bzero(recv_buf, sizeof(recv_buf));
            recv(sock_fd, recv_buf, sizeof(recv_buf), 0);
            printf("\n%s>> BALANCE: RM%s <<%s\n", BOLD_GREEN, recv_buf, RESET);
            printf("\nPress Enter to return...");
            getchar();
        } 
        else if (choice == 2) { // --- DEPOSIT ---
            printf("\n%sEnter DEPOSIT amount: RM%s", BOLD_CYAN, RESET);
            scanf("%f", &amount);
            clear_stdin();
            
            show_loading("Processing Deposit");
            sprintf(send_buf, "DEP:%f", amount); 
            send(sock_fd, send_buf, strlen(send_buf), 0);

            bzero(recv_buf, sizeof(recv_buf));
            recv(sock_fd, recv_buf, sizeof(recv_buf), 0);
            printf("%s[✔] %s%s\n", BOLD_GREEN, recv_buf, RESET);
            printf("\nPress Enter to continue...");
            getchar();
        }
        else if (choice == 3) { // --- WITHDRAWAL ---
            printf("\n%sEnter WITHDRAWAL amount: RM%s", BOLD_CYAN, RESET);
            scanf("%f", &amount);
            clear_stdin();

            show_loading("Processing Withdrawal");
            sprintf(send_buf, "WDW:%f", amount);
            send(sock_fd, send_buf, strlen(send_buf), 0);

            bzero(recv_buf, sizeof(recv_buf));
            recv(sock_fd, recv_buf, sizeof(recv_buf), 0);

            if (strncmp(recv_buf, "OK", 2) == 0) {
                printf("%s[✔] %s%s\n", BOLD_GREEN, recv_buf, RESET);
            } else {
                printf("%s[X] ERROR: %s%s\n", BOLD_RED, recv_buf, RESET);
            }
            printf("\nPress Enter to continue...");
            getchar();
        }
        else if (choice == 4) { // --- LOGOUT ---
            send(sock_fd, "LGOUT", 5, 0);
            printf("\n%sLogging out... THANK YOU!!%s\n", BOLD_CYAN, RESET);
            sleep(1);
            break;
        }
    }

    close(sock_fd);
    return 0;
}
