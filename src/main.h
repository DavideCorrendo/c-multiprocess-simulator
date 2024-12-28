
#ifndef DIRECTOR
#define DIRECTOR

#define MAX_LINE_LENGTH 100

#define SHM_KEY_STATS   123456789
#define SHM_KEY_SEATS   456789123
#define SHM_KEY_MACROS  789123456

#define MSG_KEY         345678912

#define SEM_KEY         567891234

#define MAX_MSG_SIZE    500

#define NUM_MACROS      100

#define NANOSECONDS_PER_MINUTE 60000000000ULL // 1 minute = 60 seconds = 60 billion nanoseconds

#define N_NANO_SEC 1000

#define TIMES_ARRAY {10, 8, 6, 8, 20, 20}

#define P_SERV_MIN 20 
#define P_SERV_MAX 60

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
    int worker_id;
}worker_seat;

typedef struct stats{
    int tot_num_users_tot;
    int avg_num_users_daily;
    int tot_num_tasks_done;
    int tot_num_tasks_not_done;
    int avg_num_tasks_done;
    int avg_num_tasks_not_done;
    float avg_time_users_wait_tot;
    float avg_time_users_wait_daily;
    float avg_tasks_done_tot;
    float avg_tasks_done_daily;

    int prev_stats_tot_users[6];
    int prev_stats_avg_users[6];
    int prev_stats_tot_tasks_done[6];
    int prev_stats_tot_tasks_not_done[6];
    int prev_stats_avg_tasks_done[6];
    int prev_stats_avg_tasks_not_done[6];
    float prev_stats_avg_wait_tot[6];
    float prev_stats_avg_wait_daily[6];
    float prev_stats_avg_done_tot[6];
    float prev_stats_avg_done_daily[6];

    int num_workers_active_daily;
    int num_workers_active_tot;
    int avg_num_pause_daily;
    int num_pause_tot;
    double num_ratio_worker_user[100];
}stats;

struct message {
    long mtype;
    char mtext[MAX_MSG_SIZE];
    int num;
};

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};


void initialization_shm(int *shmid_stats, int *shmid_seats, int *shmid_macros, int SIM_DURATION, int NOF_WORKERSEATS,
                       stats **shared_stats, worker_seat **shared_seats, int **shared_macros);
int leggi_parametro(const char *, const char *);
int initSem(int semId, int value);
int reserveSem(int semId);
int releaseSem(int semId);
int get_semaphore_value(int semid, int sem_num);
void print_stats(stats stat);
void tasks_assignment(worker_seat *shared_seats, stats curr_stats, int *shared_macros);
int sem_operation(int semid, int sem_num, int op_value);
int wait_semaphore(int semid, int sem_num);
int signal_semaphore(int semid, int sem_num);
int init_semaphore(int semid, int sem_num, int value);
int get_semaphore_value(int semid, int sem_num);
void working_time(worker_seat *, stats *, int *, int, int, struct message, int, int);


#endif