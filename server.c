/* server.c */
#include "common.h"

int main() {
	int sockfd, cli_len, nready, max_i, nrecv;
	struct pollfd pollfds[MAX_CLI];
	char buf[MAX_BUF];
	struct sockaddr_in serv_addr, cli_addr;

	bzero((char *)&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERV_TCP_PORT);
	serv_addr.sin_addr.s_addr = inet_addr("192.168.0.16"); /* Change server IP address here */
	
	if(fork() == 0) {
		printf("Starting logger module...\n");
		//execl("./logger", "logger", (char *) 0);
		//perror("\n!execl() failed!\n");
		exit(1);
	}
	
	printf("Creating socket...\n");
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Server: socket() error\n");
		exit(1);
	}
	
	printf("Binding socket...\n");
	if((bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
		perror("Server: bind() error\n");
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
				perror("\n!Too many clients!\n");
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
					} else perror("\n!Read error!\n");
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
					}
					
					printf("Receive message %s from client %s.\n", buf, inet_ntoa(cli_addr.sin_addr));
					break;
				}
				
				if(--nready <= 0) break;
			}
		}
	}
	close(sockfd);
}
