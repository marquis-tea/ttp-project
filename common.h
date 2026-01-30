#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <string.h>
#include <poll.h>
#include <errno.h>

#define SERV_TCP_PORT 25000
#define MAX_CLI 10
#define MAX_BUF 256

