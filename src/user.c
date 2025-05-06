#include "../include/main.h"

static int ticket_msgid = -1;      
static int *worker_msgids = NULL; 
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
int SIM_DURATION;

void office_time(struct message *msg, int msgid, int* worker_msgids ,int tasks[], int semid, int *remaining_task, int num_task, bool tasks_done[]);
void task_time(struct message msg, int msgid, int semid, int worker_id, int seat_num, bool *end_day);
void cleanup_resources();
int random_weighted(int values[], int weights[], int size);
void update_stats(int num_task, int remaining_task, int day, int *tasks, bool *tasks_done);
bool exist(int i , bool tasks_done[], int tasks[]);

void signal_handler(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL; 
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    cleanup_resources();
    
    raise(sig);
}

int main() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT");
        raise(SIGTERM);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        raise(SIGTERM);
    }
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction SIGHUP");
        raise(SIGTERM);
    }


    FILE *file = fopen("conf/config_timeout.conf", "r");
    fscanf(file, "SIM_DURATION=%d", &SIM_DURATION);
    fclose(file);

    struct message msg;

    initialize_IPC(&ticket_msgid, &worker_msgids , &semid, &shared_daily_stats, &shared_tot_stats, &shared_seats, &shared_macros);

    srand(time(NULL) + getpid());

    int values[] = {0, 1, 2, 3, 4, 5};
    int weights[] = {50, 20, 15, 10, 4, 1};
    int size_r = sizeof(values) / sizeof(values[0]);
    int day;

        while(get_semaphore_value(semid, 3) == 0) {

            int num_task = (rand() % shared_macros->N_REQUESTS) + 1;
            int remaining_task = num_task;
            int *tasks = malloc(num_task * sizeof(int));
            bool *tasks_done = malloc(num_task * sizeof(bool));
            for(int i = 0; i < num_task; i++){
                tasks[i] = random_weighted(values, weights, size_r);
                tasks_done[i] = false;
            }
            
            wait_signal(semid, 0); 
            
                day = shared_macros->current_day; 

                office_time(&msg, ticket_msgid, worker_msgids, tasks, semid, &remaining_task, num_task, tasks_done);
                wait_semaphore(semid, 4);
                update_stats(num_task, remaining_task, day, tasks, tasks_done);
                signal_semaphore(semid, 4);

                wait_semaphore(semid, 5);
                shared_macros->processes_finished++;
                shared_macros->USER_FINISHED++;
                signal_semaphore(semid, 5);

            wait_signal(semid, 1);

        }
    
    reset_signals_to_default();
    cleanup_resources();
    sleep(10);
    return 1;
}

void office_time(struct message *msg, int ticket_msgid, int* worker_msgids ,int tasks[], int semid, int *remaining_task, int num_task, bool tasks_done[]) {
    
    int seat_num;

    for(int i = 0; i < num_task && get_semaphore_value(semid, 1) == 0; i++){

        msg->mtype = 1; 

        msg->num = tasks[i];
        wait_semaphore(semid, 2);
        msgsnd(ticket_msgid, msg, sizeof(struct message) - sizeof(long), 0);
        msgrcv_wait(ticket_msgid, msg, sizeof(struct message) - sizeof(long), 2, semid);
        signal_semaphore(semid, 2);
        if(msg->num == -2){
            continue;
        }
        
        seat_num = msg->num;
        msg->mtype = 1;
        msg->num = shared_macros->timer;

        wait_semaphore(semid, num_sem + seat_num);
        msg->num = shared_macros->timer - msg->num;
        msgsnd(worker_msgids[seat_num], msg, sizeof(struct message) - sizeof(long), 0);
        msgrcv_wait(worker_msgids[seat_num], msg, sizeof(struct message) - sizeof(long), 2, semid);
        signal_semaphore(semid, num_sem + seat_num);

        if(msg->num > -1){//
        tasks_done[i] = true;
        (*remaining_task)--;}
    }

}

void cleanup_resources() {
    if (shared_seats != NULL) {
        shmdt(shared_seats);
        shared_seats = NULL;
    }
    if (shared_macros != NULL) {
        shmdt(shared_macros);
        shared_macros = NULL;
    }
}

int random_weighted(int values[], int weights[], int size) {

    int total_weight = 0;
    for (int i = 0; i < size; i++) {
        total_weight += weights[i];
    }

    int rand_num = rand() % total_weight;

    for (int i = 0; i < size; i++) {
        if (rand_num < weights[i]) {
            return values[i];
        }
        rand_num -= weights[i];
    }

    return -1;
}

void update_stats(int num_task, int remaining_task, int day, int *tasks, bool *tasks_done){

    if(remaining_task < num_task){
        shared_daily_stats[day].user_served_daily++;

        shared_tot_stats->num_user_served++;
    }

    shared_daily_stats[day].task_done += (num_task - remaining_task);
    shared_daily_stats[day].task_not_done += remaining_task;

    shared_tot_stats->num_task_done += (num_task - remaining_task);
    shared_tot_stats->num_task_not_done += remaining_task;

    for(int i = 0; i < num_task; i++){
        if(tasks_done[i]){
            shared_daily_stats[day].user_served_per_task[tasks[i]]++;
            shared_daily_stats[day].task_done_per_task[tasks[i]]++;

            if(!exist(i, tasks_done, tasks))shared_tot_stats->num_user_served_per_task[tasks[i]]++;
            shared_tot_stats->num_task_done_per_task[tasks[i]]++;
        }else{
            shared_daily_stats[day].task_not_done_per_task[tasks[i]]++;
            shared_tot_stats->num_task_not_done_per_task[tasks[i]]++;
        }
    }

}

bool exist(int i , bool tasks_done[], int tasks[]){
    bool res = false;
    for(int j = 0; j < i && !res; j++){
        if(tasks[i] == tasks[j]){
            if(tasks_done[j])res = true;   
        }
    }
    return res;
}

