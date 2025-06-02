#include "../include/main.h"

bool find_seat(int task, int id_worker, int semid, int *seat_num);
void working_time(int task, int id_worker, struct message *msg, int msgid, int semid, int pause_counter, int day);
void update_stats(int day, int task, float task_time, bool pause, int wait_time);
float ratio_worker_seats(int task);

static int ticket_msgid = -1;      
static int *worker_msgids = NULL; 
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
int SIM_DURATION;

void cleanup_resources() {
    if (shared_daily_stats && shmdt(shared_daily_stats) == -1) perror("Failed to detach shared_daily_stats");
    if (shared_tot_stats && shmdt(shared_tot_stats) == -1) perror("Failed to detach shared_tot_stats");
    if (shared_seats && shmdt(shared_seats) == -1) perror("Failed to detach shared_seats");
    if (shared_macros && shmdt(shared_macros) == -1) perror("Failed to detach shared_macros");
}

void signal_handler(int signum) {
    cleanup_resources();
    exit(signum);
}

int main(int argc, char *argv[]) {

    if(argc != 2){
        printf("too many arguments");
        raise(SIGTERM);
    }

    struct message msg;
    int id_worker = atoi(argv[1]);

    //signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    srand((time(NULL)) + getpid());
    int task = rand() % 6;//take a random task for the worker until the day_ended of simulation

    FILE *file = fopen("conf/config_timeout.conf", "r");
    fscanf(file, "SIM_DURATION=%d", &SIM_DURATION);
    fclose(file);

    initialize_IPC(&ticket_msgid, &worker_msgids , &semid, &shared_daily_stats, &shared_tot_stats, &shared_seats, &shared_macros);

    int pause_counter = 0;
    bool day_ended = false;
    int seat_num;

    wait_semaphore(semid, stats);
    shared_tot_stats->num_worker_per_task[task]++;
    signal_semaphore(semid, stats);


    while(get_semaphore_value(semid, end_simulation) == 0){

        wait_signal(semid, start_day);//wait until director declares start of the day
        day_ended = false;
        int day = shared_macros->current_day;
        seat_num = -1;

        while (!find_seat(task, id_worker, semid, &seat_num) && day_ended == false) {
            day_ended = get_semaphore_value(semid, end_day);
            if(day_ended){break;}
            sleep(1);
        }
        
        if(day_ended == false)working_time(task, seat_num, &msg, worker_msgids[seat_num], semid, pause_counter, day);

        wait_semaphore(semid, macros);
        shared_macros->processes_finished++;
        signal_semaphore(semid, macros);

        if(seat_num != -1){
            wait_semaphore(semid, seat_num + worker_seats);
            shared_seats[seat_num].busy = false;
            shared_seats[seat_num].worker_id = -1;
            signal_semaphore(semid, seat_num + worker_seats);
        }

        wait_signal(semid, end_day);//wait until director declares end of the day
        
        msg.num = -1;
        seat_num = -1;
    }

    reset_signals_to_default();
    cleanup_resources();
    sleep(10);
    exit(EXIT_SUCCESS);
}

void working_time(int task, int seat_num, struct message *msg, int msgid, int semid, int pause_counter, int day) {

    bool pause = false;
    int user_served = 0, wait_time = 0;
    float time_task_count = 0;

    while (get_semaphore_value(semid, end_day) == 0 && !pause) {
        msgrcv_wait(msgid, msg, sizeof(struct message) - sizeof(long), 1, semid);//wait until a user send a message or day finish
        if(msg->num == -1)break;//if day finished
        wait_time += msg->num;
        float time_task = msg->time;
        
        usleep((time_task * N_NANO_SECS) / 1000);//simulates working

        msg->mtype = 2;
        msg->num = 1;
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);//send message to user when finished

        user_served++;
        time_task_count += time_task;

        if (((rand() % 100) <= 1) && pause_counter < shared_macros->NOF_PAUSE) {//1% chance of doing pause after task done 
            pause_counter++;
            pause = true;
            wait_semaphore(semid, worker_seats + seat_num);
            shared_seats[seat_num].busy = false;
            shared_seats[seat_num].worker_id = -1;
            signal_semaphore(semid, worker_seats + seat_num);
        }

    }

    wait_semaphore(semid, stats);
    update_stats(day, task, time_task_count, pause, wait_time);
    signal_semaphore(semid, stats);
}

bool find_seat(int task, int id_worker, int semid, int *seat_num) {
    bool res = false;
    //cicle for every seat, if seat[i] has same task of worker and in not busy, worker takes it, otherwise it moves to the next seat
    for (int i = 0; i < shared_macros->NOF_WORKERSEATS && !res; i++) {
        wait_semaphore(semid, i + worker_seats);
        if (shared_seats[i].task == task && !shared_seats[i].busy) {
            shared_seats[i].busy = true;
            *seat_num = i;
            shared_seats[i].worker_id = id_worker;
            res = true;
        }
        signal_semaphore(semid, i + worker_seats);
    }
    return res;
}

void update_stats(int day, int task, float task_time, bool pause, int wait_time){
    
    shared_daily_stats[day].daily_waiting_time += wait_time;
    shared_daily_stats[day].time_wait_daily_per_task[task] += wait_time;

    //WAIT TIME
    shared_tot_stats->wait_time += wait_time;
    shared_tot_stats->wait_time_per_task[task] += wait_time;

    //TIME TASK
    shared_daily_stats[day].time_task_daily += task_time;
    shared_daily_stats[day].time_task_daily_per_task[task] += task_time;

    shared_tot_stats->task_time += task_time;
    shared_tot_stats->task_time_per_task[task] += task_time;
    
    if(pause == true){
        shared_daily_stats[day].num_pause_daily++;
        shared_tot_stats->num_pause++;
    }

    shared_daily_stats[day].num_workers_active_daily++;
    shared_tot_stats->num_worker_active++;

    shared_daily_stats[day].num_ratio_worker_user[task] = ratio_worker_seats(task);

}

float ratio_worker_seats(int task){
    
    int num_workerseats_per_task = 0;
    for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++){
        if(shared_seats[i].task == task){
            num_workerseats_per_task++;
        }
    }

    return (float)shared_tot_stats->num_worker_per_task[task] / num_workerseats_per_task;
}
