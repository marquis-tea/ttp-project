#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h> 

#define FIFO_NAME "atm_fifo"
#define LOG_FILE "atm_activity.log"  // The file where logs are saved
#define MSG_KEY 1234

struct msg_buffer {
    long msg_type;
    char msg_text[100];
} message;

int main() {
    int bytes_read;
    char buffer[100];
    int log_fd; // File descriptor for the log file

    int pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        exit(1);
    }

    // ---------------------------------------------------------
    // PARENT PROCESS: Handles FIFO (Receives from Server)
    // ---------------------------------------------------------
    if (pid > 0) {
        printf("[Logger Parent] Started. Handling FIFO.\n");

        // Create FIFO
        if (mkfifo(FIFO_NAME, 0666) == -1) {
            if (errno != EEXIST) {
                perror("[Logger] mkfifo failed");
                exit(1);
            }
        }

        // Open FIFO for reading
        int fifo_fd = open(FIFO_NAME, O_RDONLY);
        if (fifo_fd == -1) {
            perror("[Logger] open FIFO failed");
            exit(1);
        }

        while (1) {
            // Read from FIFO
            bytes_read = read(fifo_fd, buffer, 99);

            if (bytes_read > 0) {
                buffer[bytes_read] = '\0'; // Null-terminate just in case

                // Check for EXIT command
                if (buffer[0] == 'E' && buffer[1] == 'X' && buffer[2] == 'I' && buffer[3] == 'T') {
                    printf("[Logger] Received EXIT. Shutting down.\n");
                    close(fifo_fd);
                    break; 
                } 
                else {
                    // --- WRITE TO LOG FILE ---
                    
                    // 1. Open file (Create if missing, Append to end, Write Only)
                    log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    if (log_fd == -1) {
                        perror("[Logger] Failed to open log file");
                        // We continue loop even if log fails, to keep server running
                        continue; 
                    }

                    // 2. Write the buffer to the file
                    if (write(log_fd, buffer, bytes_read) == -1) {
                        perror("[Logger] Failed to write to file");
                    }
                    
                    // 3. Write a newline for formatting (optional but good practice)
                    write(log_fd, "\n", 1);

                    // 4. Close the file to save changes immediately
                    close(log_fd);
                    
                    printf("[Logger] Written to file: %s\n", buffer); 
                }
            } 
            else if (bytes_read == -1) {
                perror("[Logger] read failed");
            }
        }
        
        wait(NULL); // Wait for child
        printf("[Logger Parent] Exiting.\n");
    } 

    // ---------------------------------------------------------
    // CHILD PROCESS: Handles Message Queue
    // ---------------------------------------------------------
    else {
        printf("[Logger Child] Started. Handling Message Queue.\n");

        int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
        if (msgid == -1) {
            perror("[Logger Child] msgget failed");
            exit(1);
        }

        while (1) {
            // Receive message
            // msgrcv returns the number of bytes actually copied into msg_text
            int msg_len = msgrcv(msgid, &message, sizeof(message.msg_text), 0, 0);
            
            if (msg_len == -1) {
                perror("[Logger Child] msgrcv failed");
                exit(1);
            }

            // Check for EXIT
            if (message.msg_text[0] == 'E' && message.msg_text[1] == 'X' && 
                message.msg_text[2] == 'I' && message.msg_text[3] == 'T') {
                printf("[Logger Child] Received EXIT via MsgQueue.\n");
                break;
            } 
            else {
                // --- WRITE TO LOG FILE (For Message Queue too) ---
                
                log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (log_fd == -1) {
                    perror("[Logger Child] Failed to open log file");
                    continue;
                }

                // Write the message text
                write(log_fd, message.msg_text, msg_len);
                write(log_fd, "\n", 1); // Add newline

                close(log_fd);
                
                printf("[Logger Child] Written to file: %s\n", message.msg_text);
            }
        }
        
        exit(0);
    }

    return 0;
}
