int main() {
	int bytes_read;
	char buf[MAX_BUF];
	int log_fd; // File descriptor for the log file
	struct q_entry message;

	int pid = fork();
	
	if (pid < 0) {
		perror("!Fork failed!");
		exit(1);
	}
	
	// THE PARENT PROCESS RUNS FIFO
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

		for(;;) {
			bytes_read = read(fifo_fd, buf, MAX_BUF);

			if (bytes_read > 0) {
				buf[bytes_read] = '\0'; // Null-terminate just in case

				// Check for EXIT command
				if (strncmp(buf, EXIT, strlen(EXIT)) == 0) {
					printf("[Logger] Received EXIT. Shutting down.\n");
					close(fifo_fd);
					break; 
				} 
				else {
					// WRITE TO LOG FILE

					log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
					if (log_fd == -1) {
						perror("[Logger] Failed to open log file");
						// We continue loop even if log fails, to keep server running
						continue; 
					}

					if (write(log_fd, buf, bytes_read) == -1) {
						perror("[Logger] Failed to write to file");
					}

					write(log_fd, "\n", 1);
					close(log_fd);

					printf("[Logger] Written to file: %s\n", buf); 
				}
			} 
			else if (bytes_read == -1) {
				perror("[Logger] read failed");
			}
		}

		wait((int*) 0); // Wait for child
		printf("[Logger Parent] Exiting.\n");
	}

	// THE CHILD PROCESS RUNS MSG QUEUE
	else {
		printf("[Logger Child] Started. Handling Message Queue.\n");

		int msgid = msgget(MSG_KEY, 0666 | IPC_CREAT);
		if (msgid == -1) {
			perror("[Logger Child] msgget failed");
			exit(1);
		}

		for(;;) {
			// Receive message
			// msgrcv returns the number of bytes actually copied into mtext
			int msg_len = msgrcv(msgid, &message, sizeof(message.mtext), 0, 0);

			if (msg_len == -1) {
				perror("[Logger Child] msgrcv failed");
				exit(1);
			}

			// Check for EXIT
			if (strncmp(message.mtext, EXIT, strlen(EXIT)) == 0) {
				printf("[Logger Child] Received EXIT via MsgQueue.\n");
				break;
			} 
			else {
				// WRITE TO LOG FILE for message queue

				log_fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
				if (log_fd == -1) {
					perror("[Logger Child] Failed to open log file");
					continue;
				}

				// Write the message text
				write(log_fd, message.mtext, msg_len);
				write(log_fd, "\n", 1); // Add newline

				close(log_fd);

				printf("[Logger Child] Written to file: %s\n", message.mtext);
			}
		}

		exit(0);
	}

	return 0;
}
