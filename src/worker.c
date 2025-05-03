#include "../include/main.h"

bool find_seat(int task, int id_worker, int semid, int *seat_num);
void working_time(int task, int id_worker, struct message *msg, int msgid, int semid, int pause_counter, int day);
void update_stats(int day, int task, float task_time, bool pause, int wait_time);
float ratio_worker_seats(int index, int num);

static int ticket_msgid = -1;      
static int *worker_msgids = NULL; 
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
int SIM_DURATION;

// Cleanup function
void cleanup_resources() {
    if (shared_daily_stats && shmdt(shared_daily_stats) == -1) perror("Failed to detach shared_daily_stats");
    if (shared_tot_stats && shmdt(shared_tot_stats) == -1) perror("Failed to detach shared_tot_stats");
    if (shared_seats && shmdt(shared_seats) == -1) perror("Failed to detach shared_seats");
    if (shared_macros && shmdt(shared_macros) == -1) perror("Failed to detach shared_macros");
}

// Signal handler
void signal_handler(int signum) {
    cleanup_resources();
    exit(signum);
}

int main(int argc, char *argv[]) {

    if(argc != 2){
        printf("too many arguments");
        raise(SIGTERM);
    }

    //printf("[%d] worker iniziato \n", getpid()); 

    struct message msg;
    int id_worker = atoi(argv[1]);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    srand((time(NULL)) + getpid());
    int task = rand() % 6;

    FILE *file = fopen("conf/config_timeout.conf", "r");
    fscanf(file, "SIM_DURATION=%d", &SIM_DURATION);
    fclose(file);

    //initializing all IPC structures of worker
    initialize_IPC(&ticket_msgid, &worker_msgids , &semid, &shared_daily_stats, &shared_tot_stats, &shared_seats, &shared_macros);

    int pause_counter = 0;
    memset(&msg, 0, sizeof(msg));

    bool end = false;
    int seat_num;

    wait_semaphore(semid, 4);
    shared_tot_stats->num_worker_per_task[task]++;
    signal_semaphore(semid, 4);

    while(get_semaphore_value(semid, 3) == 0){

        wait_signal(semid, 0); 
        end = false;
        int day = shared_macros->current_day;
        seat_num = -1;

        while (!find_seat(task, id_worker, semid, &seat_num) && end == false) {
            //printf("[worker %d] qui\n", getpid());
            end = get_semaphore_value(semid, 1);
            //printf("[worker %d] qua\n", getpid());
            if(end){break;}
            //usleep((N_NANO_SEC * 10) / 1000);
            sleep(1);
        }
        //printf("[worker %d] findseat finita end = %d\n", getpid(), end);
        if(end == false)working_time(task, seat_num, &msg, worker_msgids[seat_num], semid, pause_counter, day);

        wait_semaphore(semid, 5);
        shared_macros->processes_finished++;
        signal_semaphore(semid, 5);

        if(seat_num != -1){
            wait_semaphore(semid, seat_num + num_sem);
            shared_seats[seat_num].busy = false;
            shared_seats[seat_num].worker_id = -1;
            signal_semaphore(semid, seat_num + num_sem);
        }

        wait_signal(semid, 1);
        //printf("[worker %d] worker finito\n", getpid());
        
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

    while (get_semaphore_value(semid, 1) == 0 && !pause) {
        //printf("[worker %d] seat_num = %d con task = %d\n", getpid(), seat_num, task);
        msgrcv_wait(msgid, msg, sizeof(struct message) - sizeof(long), 1, semid);
        if(msg->num == -1)break;
        //printf("[worker %d] ricevuto messaggio\n", getpid());
        wait_time += msg->num;
        float time_task = msg->time;
        
        usleep((time_task * N_NANO_SECS) / 1000);

        msg->mtype = 2;
        msg->num = 1;//
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);

        user_served++;
        //printf("TASK = %d ----- TIME = %.2f\n", task, time_task);
        time_task_count += time_task;

        if (((rand() % 100) <= 1) && pause_counter < shared_macros->NOF_PAUSE) {
            pause_counter++;
            pause = true;
            wait_semaphore(semid, 6);
            shared_seats[seat_num].busy = false;
            shared_seats[seat_num].worker_id = -1;
            signal_semaphore(semid, 6);
        }

    }

    //usleep(500);
    wait_semaphore(semid, 4);
    //printf("STATS: %d, %d\n", time_task_count, wait_time);
    update_stats(day, task, time_task_count, pause, wait_time);
    signal_semaphore(semid, 4);
}

bool find_seat(int task, int id_worker, int semid, int *seat_num) {
    bool res = false;
    for (int i = 0; i < shared_macros->NOF_WORKERSEATS && !res; i++) {
        //printf("[worker %d] credo qui\n", getpid());
        wait_semaphore(semid, i + num_sem);
        //printf("[worker %d] credo qua\n", getpid());
        if (shared_seats[i].task == task && !shared_seats[i].busy) {
            shared_seats[i].busy = true;
            *seat_num = i;
            shared_seats[i].worker_id = id_worker;
            res = true;
        }
        signal_semaphore(semid, i + num_sem);
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

    for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++){
        int current_task = shared_seats[i].task;
        int workers_for_task = shared_tot_stats->num_worker_per_task[current_task];
        shared_tot_stats->num_ratio_worker_user[i] = ratio_worker_seats(current_task, workers_for_task);
    }
}

float ratio_worker_seats(int task, int num){
    
    int num_workerseats_per_task = 0;
    for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++){
        if(shared_seats[i].task == task){
            num_workerseats_per_task++;
        }
    }

    if(num_workerseats_per_task != 0)return (float)num / num_workerseats_per_task;
    return 0;
}
