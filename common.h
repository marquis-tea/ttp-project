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
#include <time.h>

#define SERV_TCP_PORT 25000
#define MAX_CLI 10
#define MAX_BUF 256
#define MAX_ALRT 50
#define ERR_PIN "ERR_PIN"
#define ERR_ID "ERR_ID"
#define ERR_BAL "ERR_BAL"
#define OK "OK"
#define VAL "VAL"
#define LGN "LGN"
#define BAL "BAL"
#define WDW "WDW"
#define DEP "DEP"
#define LGOUT "LGOUT"
#define delim '|'

struct q_entry {
	long mtype;
	char mtext[MAX_ALRT];
};
