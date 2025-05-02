#include "main.h"

static int msgid = -1;
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
int SIM_DURATION;

void office_time(struct message *msg, int msgid, int tasks[], int semid, int *remaining_task, int num_task, bool tasks_done[]);
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

    //printf("[%d] user iniziato \n", getpid());

    FILE *file = fopen("config_timeout.conf", "r");
    fscanf(file, "SIM_DURATION=%d", &SIM_DURATION);
    fclose(file);

    struct message msg;

    initialize_IPC(&msgid, &semid, &shared_daily_stats, &shared_tot_stats, &shared_seats, &shared_macros);

    srand(time(NULL) + getpid());

    int values[] = {0, 1, 2, 3, 4, 5};
    int weights[] = {50, 20, 15, 10, 4, 1};
    int size_r = sizeof(values) / sizeof(values[0]);
    int day;

    //<-------------------------SEMAPHORE FOR THE TICKETS EROGATOR - USERS COMUNICATION GESTION-------------------------------->


        while(get_semaphore_value(semid, 3) == 0) {

            //printf("%d iniziato \n", getpid());

            //int P_SERV = rand() % (shared_macros->P_SERVE_MAX - shared_macros->P_SERVE_MIN + 1) + shared_macros->P_SERVE_MIN;//probabilty to go to the office
            
            //int decision = rand() % 101;
            int num_task = (rand() % shared_macros->N_REQUESTS) + 1;
            int remaining_task = num_task;
            int *tasks = malloc(num_task * sizeof(int));
            bool *tasks_done = malloc(num_task * sizeof(bool));
            for(int i = 0; i < num_task; i++){
                tasks[i] = random_weighted(values, weights, size_r);
                tasks_done[i] = false;
            }
            //int time = rand() % 480;---------
            //printf("[user: %d] qui\n", getpid());
            wait_signal(semid, 0); 
            //printf("[user: %d] qua\n", getpid());
            //usleep((time * N_NANO_SECS) / 1000);--------------
            
            //if(decision <= P_SERV){
                /*printf("[user %d] num_task: %d\n", getpid(), num_task);
                for(int i = 0; i < num_task; i++){
                    printf("[user %d] tasks[%d]: %d\n", getpid(), i, tasks[i]);
                }*/
                day = shared_macros->current_day; 

                office_time(&msg, msgid, tasks, semid, &remaining_task, num_task, tasks_done);
                wait_semaphore(semid, 4);
                update_stats(num_task, remaining_task, day, tasks, tasks_done);
                signal_semaphore(semid, 4);

                wait_semaphore(semid, 5);
                shared_macros->processes_finished++;
                shared_macros->USER_FINISHED++;
                signal_semaphore(semid, 5);
            //}

            wait_signal(semid, 1);

           
        }
    
    reset_signals_to_default();
    cleanup_resources();
    sleep(10);
    return 1;
}

void office_time(struct message *msg, int msgid, int tasks[], int semid, int *remaining_task, int num_task, bool tasks_done[]) {
    
    int seat_num;

    for(int i = 0; i < num_task && get_semaphore_value(semid, 1) == 0; i++){

        msg->mtype = 1; 
        /*printf("[user %d] taks[%d] = %d\n", getpid(), i, tasks[i]);

        for(int j = 0; j < shared_macros[1]; j++){
            printf("[user %d] %d OCCUPATO È %d\n", getpid(), j, shared_seats[j].busy);
        }*/

        msg->num = tasks[i];
        wait_semaphore(semid, 2);
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);
        //printf("[user %d] mandato messaggio a t.e. : %d tipo: %ld\n", getpid(), msg->num, msg->mtype);
        msgrcv_wait(msgid, msg, sizeof(struct message) - sizeof(long), 2, semid);
        //printf("[user %d] ricevuto messaggio da t.e. : %d tempo: %.10f\n", getpid(), msg->num, msg->time);
        signal_semaphore(semid, 2);
        if(msg->num == -2){
            //printf("[user %d] ricevuto -1 aspettando %d\n", getpid(), tasks[i]);
            continue;
        }
        
        seat_num = msg->num;
        msg->mtype = seat_num + 3;
        msg->num = shared_macros->timer;
        //printf("[user %d] inizio ad aspettare a temp %d\n", getpid(), shared_macros->timer);
        //printf("[user %d] RICEVUTA SEDIA %d\n", getpid(), seat_num);
        //puts("ASPETTO WAIT");
        wait_semaphore(semid, num_sem + seat_num);
        //puts("FINITO ASPETTO");
        //printf("[user %d] finito ad aspettare a temp %d\n", getpid(), shared_macros->timer);
        msg->num = shared_macros->timer - msg->num;
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);
        //printf("[user %d]messaggio mandato a %lu\n", getpid(), msg->mtype);
        msgrcv_wait(msgid, msg, sizeof(struct message) - sizeof(long), (seat_num + shared_macros->NOF_WORKERSEATS) + 3, semid);
        //printf("[user %d] ricevuto messaggio da worker %d\n", getpid (), msg->num);
        signal_semaphore(semid, num_sem + seat_num);

        //printf("[user] ricevuto tipo %lu messaggio %s\n", msg->mtype, msg->mtext);
        tasks_done[i] = true;
        (*remaining_task)--;
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

    if(shared_daily_stats[day].user_served_daily > 0){shared_daily_stats[day].avg_num_users_daily = (float)shared_daily_stats[day].user_served_daily / shared_macros->NOF_WORKERS;}
    if(shared_daily_stats[day].task_done > 0) shared_daily_stats[day].avg_num_tasks_done_daily = (float)shared_daily_stats[day].task_done / shared_macros->NOF_WORKERS;
    if(shared_daily_stats[day].task_not_done > 0)shared_daily_stats[day].avg_num_tasks_not_done_daily = (float)shared_daily_stats[day].task_not_done / shared_macros->NOF_WORKERS;

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

