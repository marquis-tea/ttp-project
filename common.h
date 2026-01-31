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
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/sem.h>

/* Server Config */
#define SERV_TCP_PORT 25000
#define MAX_CLI 10
#define MAX_BUF 256
#define MAX_ALRT 50

#define MAX_FIELD 50
#define SEM_KEY 1234

/* Commands and Responses */
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
#define FILE_DELIM "|"
#define MSG_DELIM ":"

/* Client Config */
#define TIMEOUT 300
#define ID_LEN 8
#define PIN_LEN 6

/* Logger Config */
#define FIFO_NAME "atm_fifo"
#define LOG_FILE "log.txt"
#define MSG_KEY 1234
#define EXIT "EXIT"

struct q_entry {
	long mtype;
	char mtext[MAX_ALRT];
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
