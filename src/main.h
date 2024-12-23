
#ifndef DIRECTOR
#define DIRECTOR

#define MAX_LINE_LENGTH 100

#define SHM_KEY_STATS 123456789
#define SHM_KEY_SEATS 456789123
#define SHM_KEY_TIMER 789123456

#define MSG_KEY       345678912

#define SEM_KEY       567891234

#define MAX_MSG_SIZE  500

#define NANOSECONDS_PER_MINUTE 60000000000ULL // 1 minute = 60 seconds = 60 billion nanoseconds


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/signal.h>

#define _POSIX_C_SOURCE 200809L

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1 
#endif

enum tasks{
        send_receive_parcels = 1,
        send_receive_letters_registered,
        withdrawals_deposits,
        bill_payments,
        purchase_financial_products,
        purchase_watches_bracelets,
};

typedef struct worker_seat{
    size_t id;
    enum tasks task;
    bool busy;
}worker_seat;

typedef struct stats{
    int     tot_num_users;
    int     avg_num_users;
    int     tot_num_tasks_done;
    int     tot_num_tasks_not_done;
    int     avg_num_tasks_done;
    int     avg_num_tasks_not_done;
    clock_t avg_time_users_wait_tot;
    clock_t avg_time_users_wait_daily;
    clock_t avg_tasks_done_tot;
    clock_t avg_tasks_done_daily;
    stats   **prec_stats;
    int     num_workers_active_daily;
    int     num_workers_active_tot;
    int     avg_num_pause_daily;
    int     num_pause_tot;
    float   *num_ratio_worker_user;
}stats;

struct message {
    long mtype;
    char mtext[MAX_MSG_SIZE];
    int MACRO;
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

void initialization_shm(int *, int *, int, int, stats **, worker_seat **);
int leggi_parametro(const char *, const char *);
int initSem(int semId, int value);
int reserveSem(int semId);
int releaseSem(int semId);

#endif