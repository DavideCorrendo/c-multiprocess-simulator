#ifndef DIRECTOR
#define DIRECTOR

#define _GNU_SOURCE

#define NUM_TASKS 6
#define TIMES_ARRAY {10, 8, 6, 8, 20, 20}
#define N_NANO_SECS 10000000 //MUST BE A MULTIPLE OF 1000

#define FTOK_PATH "/tmp"
#define FTOK_DAILY_STATS 'A'
#define FTOK_TOT_STATS 'B'
#define FTOK_SEATS 'C'
#define FTOK_MACROS 'D'
#define FTOK_SEM 'E'
#define FTOK_TICKET 'F'
#define FTOK_WORKER_BASE 'G'

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1 
#endif

enum sem{
    start_day,
    end_day,
    ticket_erogator,
    end_simulation,
    stats,
    macros,
    worker_seats
};

typedef struct worker_seat{
    int id;
    int task;
    bool busy;
    int worker_id;
}worker_seat;

typedef struct daily_stats{
    int user_served_daily;
    int task_done;
    int task_not_done;
    int daily_waiting_time;
    int time_task_daily;

    int user_served_per_task[NUM_TASKS];
    int task_done_per_task[NUM_TASKS];
    int task_not_done_per_task[NUM_TASKS];
    float time_task_daily_per_task[NUM_TASKS];
    float time_wait_daily_per_task[NUM_TASKS];

    int num_pause_daily;

    int num_workers_active_daily;
    float num_ratio_worker_user[NUM_TASKS];
}daily_stats;

typedef struct tot_stats{
    int wait_time;
    int task_time;

    int num_user_served;
    int num_task_done;
    int num_task_not_done;

    int num_worker_per_task[NUM_TASKS];
    float wait_time_per_task[NUM_TASKS];
    float task_time_per_task[NUM_TASKS];

    int num_user_served_per_task[NUM_TASKS];
    int num_task_done_per_task[NUM_TASKS];
    int num_task_not_done_per_task[NUM_TASKS];

    int num_worker_active;
    int num_pause;
}tot_stats;

typedef struct shared_data{
    int NOF_WORKERS;
    int NOF_WORKERSEATS;
    int timer;
    int NOF_PAUSE;
    int N_REQUESTS;
    int NOF_USERS;
    int current_day;
    int processes_finished;
    int P_SERV_MIN;
    int P_SERV_MAX;
    int USER_FINISHED;
    int SIM_DURATION;
}shared_data;

struct message {
    long mtype;
    int num;
    float time;
};

extern union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;

int read_parameter(const char *, const char *);
int wait_semaphore(int semid, int sem_num);
int signal_semaphore(int semid, int sem_num);
int init_semaphore(int semid, int sem_num, int value);
int get_semaphore_value(int semid, int sem_num);
void initialize_IPC(int* msgid_ticket, int **msgid, int *semid, daily_stats **shared_daily_stats, tot_stats **shared_tot_stats, worker_seat **shared_seats, shared_data **shared_macros);  
void initialize_keys_modified(key_t  *shm_data_key,key_t  *sem_key,key_t *msg_key, key_t *shm_seats_key);
int sem_operation(int semid, int sem_num, int op_value);
void reset_signals_to_default();
void wait_signal(int semid, int sem_num);
void msgrcv_wait(int msgid, struct message *msg, size_t size, long type, int semid);
int random_weighted(int values[], int weights[], int size);
bool exist(int i, bool tasks_done[], int tasks[]);

#endif