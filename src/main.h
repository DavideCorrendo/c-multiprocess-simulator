#ifndef DIRECTOR
#define DIRECTOR

#define _GNU_SOURCE

#define MAX_LINE_LENGTH 100

#define MAX_MSG_SIZE    500

#define NUM_MACROS      100

#define NANOSECONDS_PER_MINUTE 60000000000ULL // 1 minute = 60 seconds = 60 billion nanoseconds

#define N_NANO_SEC 1000000

#define TIMES_ARRAY {10, 8, 6, 8, 20, 20}

#define P_SERV_MIN 20 
#define P_SERV_MAX 60

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

typedef struct worker_seat{
    int id;
    int task;
    bool busy;
    int worker_id;
}worker_seat;

typedef struct daily_stats{

    int user_served_daily;
    int user_not_served_daily;
    int daily_waiting_time;
    int time_task_daily;

    float avg_num_users_daily;
    float avg_num_tasks_done_daily;
    float avg_num_tasks_not_done_daily;
    float avg_time_users_wait_daily;
    float avg_time_tasks_done_daily;

    int user_served_per_task[6];
    int user_not_served_per_task[6];
    int time_task_daily_per_task[6];
    int time_wait_daily_per_task[6];

    float avg_num_users_daily_per_task[6];
    float avg_num_tasks_done_daily_per_task[6];
    float avg_num_tasks_not_done_daily_per_task[6];
    float avg_time_users_wait_daily_per_task[6];
    float avg_time_tasks_done_daily_per_task[6];

    int num_pause_daily;

    int num_workers_active_daily;
    float avg_num_pause_daily;
    float num_ratio_worker_user[100];

}daily_stats;

typedef struct tot_stats{

    int wait_time;
    int task_time;

    int num_user_served;
    int num_task_done;
    int num_task_not_done;
    float avg_time_wait;
    float avg_time_task;

    int wait_time_per_task[6];
    int task_time_per_task[6];

    int num_user_served_per_task[6];
    int num_task_done_per_task[6];
    int num_task_not_done_per_task[6];
    float avg_time_wait_per_task[6];
    float avg_time_task_per_task[6];

    int num_worker_active;
    int num_pause;

}tot_stats;


struct message {
    long mtype;
    char mtext[MAX_MSG_SIZE];
    int num;
};

extern union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;


int leggi_parametro(const char *, const char *);
int initSem(int semId, int value);
int wait_semaphore(int semid, int sem_num);
int signal_semaphore(int semid, int sem_num);
int init_semaphore(int semid, int sem_num, int value);
int get_semaphore_value(int semid, int sem_num);
void get_function(int *, int *, int *, int *, int *, int *);
void initialize_keys(key_t *shm_daily_stat_key, key_t *shm_tot_stat_key, key_t *shm_seats_key, key_t *shm_macros_key, key_t *sem_key, key_t *msg_key);
int num_user_waiting(int semid, int *shared_macros);
void signal_handler(int sig);
void handle_child_exit(int sig);
void handle_termination(int sig);
void cleanup(void);
void initialize_keys_modified(key_t  *shm_macros_key,key_t  *sem_key,key_t *msg_key, key_t *shm_seats_key);
int sem_operation(int semid, int sem_num, int op_value);


#endif