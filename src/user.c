#include "../include/main.h"

volatile sig_atomic_t keep_running = 1;

static int ticket_msgid = -1;      
static int *worker_msgids = NULL; 
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
int SIM_DURATION;

void office_time(struct message *msg, int msgid, int* worker_msgids ,int tasks[], int semid, int *remaining_task, int num_task, bool tasks_done[]);
void cleanup_resources();
void update_stats(int num_task, int remaining_task, int day, int *tasks, bool *tasks_done);

void signal_handler(int sig) {
    (void)sig;
    keep_running = 0;
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
    if(file){
        fscanf(file, "SIM_DURATION=%d", &SIM_DURATION);
        fclose(file);
    }

    struct message msg;

    initialize_IPC(&ticket_msgid, &worker_msgids , &semid, &shared_daily_stats, &shared_tot_stats, &shared_seats, &shared_macros);

    srand(time(NULL) + getpid());

    int values[] = {0, 1, 2, 3, 4, 5};
    int weights[] = {50, 20, 15, 10, 4, 1};
    int size_r = sizeof(values) / sizeof(values[0]);

    int day;
    wait_semaphore(semid, macros);
    int probability = (rand() % (shared_macros->P_SERV_MAX - shared_macros->P_SERV_MIN + 1)) + shared_macros->P_SERV_MIN;
    signal_semaphore(semid, macros); 

    while(get_semaphore_value(semid, end_simulation) == 0 && keep_running) {

        wait_semaphore(semid, macros); 
        int num_task = (rand() % shared_macros->N_REQUESTS) + 1;
        signal_semaphore(semid, macros); 

        int remaining_task = num_task;
        
        int *tasks = malloc(num_task * sizeof(int));
        bool *tasks_done = malloc(num_task * sizeof(bool));

        for(int i = 0; i < num_task; i++){
            tasks[i] = random_weighted(values, weights, size_r);
            tasks_done[i] = false;
        }

        int rand_decision = rand() % 100;
        
        wait_signal(semid, start_day); 

        wait_semaphore(semid, macros);
        day = shared_macros->current_day;
        signal_semaphore(semid, macros); 

        if (rand_decision < probability){
            int delay_units = rand() % 720;
            usleep((delay_units * N_NANO_SECS) / 1000); 
            office_time(&msg, ticket_msgid, worker_msgids, tasks, semid, &remaining_task, num_task, tasks_done);
        
            wait_semaphore(semid, stats);
            update_stats(num_task, remaining_task, day, tasks, tasks_done);
            signal_semaphore(semid, stats);
        }

        wait_semaphore(semid, macros);
        shared_macros->processes_finished++;
        shared_macros->USER_FINISHED++;
        signal_semaphore(semid, macros);

        free(tasks);
        free(tasks_done);

        wait_signal(semid, end_day);
    }
    
    reset_signals_to_default();
    cleanup_resources();
    sleep(1);
    return EXIT_SUCCESS;
}

void office_time(struct message *msg, int ticket_msgid, int* worker_msgids ,int tasks[], int semid, int *remaining_task, int num_task, bool tasks_done[]) {
    int seat_num;

    for(int i = 0; i < num_task && get_semaphore_value(semid, 1) == 0 && keep_running; i++){
        msg->mtype = 1; 
        msg->num = tasks[i];
        
        wait_semaphore(semid, 2);
        msgsnd(ticket_msgid, msg, sizeof(struct message) - sizeof(long), 0);
        msgrcv_wait(ticket_msgid, msg, sizeof(struct message) - sizeof(long), 2, semid);
        signal_semaphore(semid, 2);
        
        if(msg->num == -2) {
            continue;
        }
        
        seat_num = msg->num;
        msg->mtype = 1;
        msg->num = shared_macros->timer;

        wait_semaphore(semid, worker_seats + seat_num);
        msg->num = shared_macros->timer - msg->num;
        msgsnd(worker_msgids[seat_num], msg, sizeof(struct message) - sizeof(long), 0);
        msgrcv_wait(worker_msgids[seat_num], msg, sizeof(struct message) - sizeof(long), 2, semid);
        signal_semaphore(semid, worker_seats + seat_num);

        if(msg->num > -1){
            tasks_done[i] = true;
            (*remaining_task)--;
        }
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