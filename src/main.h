#ifndef DIRECTOR
#define DIRECTOR

#define _GNU_SOURCE

#define TIMES_ARRAY {10, 8, 6, 8, 20, 20}
#define N_NANO_SECS 1500000
#define num_sem 6

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

    int user_served_daily;//
    int task_done;//
    int task_not_done;//
    int daily_waiting_time;//
    int time_task_daily;//

    float avg_num_users_daily;
    float avg_num_tasks_done_daily;
    float avg_num_tasks_not_done_daily;
    float avg_time_users_wait_daily;
    float avg_time_tasks_done_daily;

    int user_served_per_task[6];//
    int task_done_per_task[6];//
    int task_not_done_per_task[6];//
    int time_task_daily_per_task[6];//
    int time_wait_daily_per_task[6];//

    float avg_num_users_daily_per_task[6];
    float avg_num_tasks_done_daily_per_task[6];
    float avg_num_tasks_not_done_daily_per_task[6];
    float avg_time_users_wait_daily_per_task[6];
    float avg_time_tasks_done_daily_per_task[6];

    int num_pause_daily;

    int num_workers_active_daily;
    float avg_num_pause_daily;

}daily_stats;

typedef struct tot_stats{

    int wait_time;//
    int task_time;//

    int num_user_served;
    int num_task_done;
    int num_task_not_done;
    float avg_time_wait;
    float avg_time_task;

    int num_worker_per_task[6];
    int wait_time_per_task[6];//
    int task_time_per_task[6];//

    int num_user_served_per_task[6];
    int num_task_done_per_task[6];
    int num_task_not_done_per_task[6];
    float avg_time_wait_per_task[6];
    float avg_time_task_per_task[6];

    int num_worker_active;
    int num_pause;

    double num_ratio_worker_user[100];//VA MESSO IN DAILY---------------------------------------
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
    int P_SERVE_MIN;
    int P_SERVE_MAX;
    int N_NEW_USERS;
}shared_data;

struct message {
    long mtype;
    int num;
};

extern union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
} arg;


int leggi_parametro(const char *, const char *);
int wait_semaphore(int semid, int sem_num);
int signal_semaphore(int semid, int sem_num);
int init_semaphore(int semid, int sem_num, int value);
int get_semaphore_value(int semid, int sem_num);
void initialize_keys(key_t *shm_daily_stat_key, key_t *shm_tot_stat_key, key_t *shm_seats_key, key_t *shm_data_key, key_t *sem_key, key_t *msg_key);
void initialize_keys_modified(key_t  *shm_data_key,key_t  *sem_key,key_t *msg_key, key_t *shm_seats_key);
int sem_operation(int semid, int sem_num, int op_value);
void reset_signals_to_default();
void wait_signal(int semid, int sem_num);
void msgrcv_wait(int msgid, struct message *msg, size_t size, long type, int semid);

#endif
