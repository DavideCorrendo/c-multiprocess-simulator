#ifndef DIRECTOR
#define DIRECTOR

#define _GNU_SOURCE

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
//#define P_SERV (rand() % (P_SERV_MAX -  P_SERV_MIN + 1) + P_SERV_MIN)  <----------------------------DA VEDERE SE GIUSTO A CAUSA DELLA DIVERSITA CHE DEVE ESSERCI TRA LE VA RIE P_SERV

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1 
#endif

typedef struct worker_seat{
    size_t id;
    int task;
    bool busy;
    int worker_id;
}worker_seat;

typedef struct stats{

    int user_served_daily;
    int user_not_served_daily;
    int daily_waiting_time;
    int time_task_daily;

    int avg_num_users_daily;
    int tot_num_tasks_not_done;
    int avg_num_tasks_done;
    int avg_num_tasks_not_done;
    float avg_time_users_wait_tot;
    float avg_time_users_wait_daily;
    float avg_time_tasks_done_tot;
    float avg_time_tasks_done_daily;

    int prev_user_served_daily[6];
    int prev_time_task_daily[6];
    int prev_time_task_tot[6];
    int prev_time_wait_daily[6];
    int prev_time_wait_tot[6];

    int prev_stats_tot_users[6];
    int prev_stats_avg_users[6];
    int prev_stats_tot_tasks_done[6];
    int prev_stats_tot_tasks_not_done[6];  //
    int prev_stats_avg_tasks_done[6];
    int prev_stats_avg_tasks_not_done[6]; //
    float prev_stats_avg_wait_tot[6];
    float prev_stats_avg_wait_daily[6];
    float prev_stats_avg_done_tot[6];
    float prev_stats_avg_done_daily[6];

    int num_pause_daily_tot;

    int num_workers_active_daily;
    int avg_num_pause_daily;
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
}arg;


void initialization_shm(int *, int *, int *, int , int , stats **, worker_seat **, int **);
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
int worker_per_task(int* , worker_seat *, int );
void get_function(int *, int *, int *, int *, int *);
bool find_seat(worker_seat *, int *, int , int , int );
void update_stats(stats *, int , int , int *, int task, worker_seat *, int , int , bool);
float ratio_worker_seats(int , int *, worker_seat *);
int num_user_waiting(int semid, int *shared_macros);



#endif