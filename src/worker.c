#include "main.h"

bool find_seat(int task, int id_worker, int semid, int *seat_num);
void working_time(int task, int avg_time_task, int id_worker, struct message *msg, int msgid, int semid, int pause_counter, int day);
void update_stats(int day, int task, int task_time, bool pause, int wait_time);
float ratio_worker_seats(int index, int num);

static int shmid_daily_stats = -1;
static int shmid_tot_stats = -1;
static int shmid_seats = -1;
static int shmid_macros = -1;
static int msgid = -1;
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static int *shared_macros = NULL;

// Cleanup function
void cleanup_resources() {
    if (shared_daily_stats && shmdt(shared_daily_stats) == -1)
        perror("Failed to detach shared_daily_stats");
    if (shared_tot_stats && shmdt(shared_tot_stats) == -1)
        perror("Failed to detach shared_tot_stats");
    if (shared_seats && shmdt(shared_seats) == -1)
        perror("Failed to detach shared_seats");
    if (shared_macros && shmdt(shared_macros) == -1)
        perror("Failed to detach shared_macros");
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

    struct message msg;

    int id_worker = atoi(argv[1]);


    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    srand((time(NULL)) + getpid());
    int task = rand() % 6;
    
    key_t shm_daily_stat_key;
    key_t shm_tot_stat_key;
    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys(&shm_daily_stat_key, &shm_tot_stat_key, &shm_seats_key, &shm_macros_key, &sem_key, &msg_key);

    shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0);
    //printf("[worker] shmid_macros = %d\n", shmid_macros);
    if(shmid_macros == -1){
        perror("shmget in worker for macros");
        raise(SIGTERM);
    }
    shared_macros = shmat(shmid_macros, NULL, 0);
    if (shared_macros == (void *)-1) {
        perror("shmat failed for macros in worker");
        raise(SIGTERM);
    }

    shmid_daily_stats = shmget(shm_daily_stat_key, shared_macros[3] * sizeof(daily_stats), 0);
    if(shmid_daily_stats == -1){
        perror("shmget in worker for daily stats");
        raise(SIGTERM);
    }
    shared_daily_stats = shmat(shmid_daily_stats, NULL, 0);
    if (shared_daily_stats == (void *)-1) {
        perror("shmat failed for stats in worker");
        raise(SIGTERM);
    }

    shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), 0);
    if(shmid_tot_stats == -1){
        perror("shmget in worker for tot stats");
        raise(SIGTERM);
    }
    shared_tot_stats = shmat(shmid_tot_stats, NULL, 0);
    if (shared_tot_stats == (void *)-1) {
        perror("shmat failed for stats in worker");
        raise(SIGTERM);
    }

    shmid_seats = shmget(shm_seats_key, shared_macros[1], 0);
    if(shmid_seats == -1){
        perror("shmget in worker for seats");
        raise(SIGTERM);
    }
    shared_seats = shmat(shmid_seats, NULL, 0);
    if (shared_seats == (void *)-1) {
        perror("shmat failed for seats in workerg");
        raise(SIGTERM);
    }

    semid = semget(sem_key, shared_macros[1] + 3, 0);
    if(semid == -1){
        perror("semid");
        raise(SIGTERM);
    }

    msgid = msgget(msg_key, 0);
    if(msgid == -1){
        perror("msgget");
        raise(SIGTERM);
    }

    int time_tasks[6] = TIMES_ARRAY;
    int pause_counter = 0;
    memset(&msg, 0, sizeof(msg));

    bool end = false;
    int seat_num = -1;

    wait_semaphore(semid, shared_macros[1]);
    shared_tot_stats->num_worker_per_task[task]++;
    signal_semaphore(semid, shared_macros[1]);

    while(strcmp(msg.mtext, "end_simulation") != 0){

        msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0);
        end = false;
        int day = shared_macros[8];

        while (!find_seat(task, id_worker, semid, &seat_num) && end == false) {
            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 2, IPC_NOWAIT);
            if(strcmp(msg.mtext, "end") == 0){end = true; break;}
            usleep((N_NANO_SEC * 10) / 1000);
        }
        if(end == false)working_time(task, time_tasks[task], seat_num, &msg, msgid, semid, pause_counter, day);

        wait_semaphore(semid, shared_macros[1] + 1);
        shared_macros[5]++;
        signal_semaphore(semid, shared_macros[1] + 1);
        wait_semaphore(semid, shared_macros[1] + 3);
        msg.mtext[0] = '\0';
        msg.num = 0;

        if(seat_num != -1){
            wait_semaphore(semid, seat_num);
            shared_seats[seat_num].busy = false;
            shared_seats[seat_num].worker_id = -1;
            signal_semaphore(semid, seat_num);
        }
        seat_num = -1;

        msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 5, 0);

        change_msg(&msgid);

    }

    reset_signals_to_default();
    cleanup_resources();
    sleep(10);
    exit(EXIT_SUCCESS);
}

void working_time(int task, int avg_time_task, int seat_num, struct message *msg, int msgid, int semid, int pause_counter, int day) {

    bool pause = false;
    int user_served = 0, time_task_count = 0, wait_time = 0;

    while (strcmp(msg->mtext, "end") != 0 && !pause) {
        msgrcv(msgid, msg, sizeof(struct message) - sizeof(long), 6 + seat_num, 0);
        puts("[worker] ricevuto messaggio");
        if(msg->num == -1)break;
        wait_time += msg->num;

        float time_task = ((float)rand() / RAND_MAX) + 0.5;
        time_task *= (float)avg_time_task;
        usleep((time_task * N_NANO_SEC) / 1000);

        strcpy(msg->mtext, "done");
        msg->mtype = 6 + seat_num;
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);

        user_served++;
        time_task_count += time_task;

        if (((rand() % 100) <= 10) && pause_counter < shared_macros[4]) {
            pause_counter++;
            pause = true;
            wait_semaphore(semid, seat_num);
            shared_seats[seat_num].busy = false;
            shared_seats[seat_num].worker_id = -1;
            signal_semaphore(semid, seat_num);
        }

        msgrcv(msgid, msg, sizeof(struct message), 2, IPC_NOWAIT);
    }

    usleep(500);
    wait_semaphore(semid, shared_macros[1]);
    update_stats(day, task, time_task_count, pause, wait_time);
    signal_semaphore(semid, shared_macros[1]);
}

bool find_seat(int task, int id_worker, int semid, int *seat_num) {
    bool res = false;
    for (int i = 0; i < shared_macros[1]; i++) {
        wait_semaphore(semid, i);
        if (shared_seats[i].task == task && !shared_seats[i].busy) {
            if (!shared_seats[i].busy) {
                shared_seats[i].busy = true;
                *seat_num = i;
                shared_seats[i].worker_id = id_worker;
                res = true;
            }
        }
        signal_semaphore(semid, i);
    }
    return res;
}

void update_stats(int day, int task, int task_time, bool pause, int wait_time){

    int num = shared_tot_stats->num_worker_per_task[task];
    
    shared_daily_stats[day].daily_waiting_time += wait_time;
    shared_daily_stats[day].time_wait_daily_per_task[task] += wait_time;

    //WAIT TIME
    shared_tot_stats->wait_time += wait_time;
    if(shared_tot_stats->num_task_done != 0)shared_tot_stats->avg_time_wait = shared_tot_stats->wait_time / shared_tot_stats->num_task_done;
    shared_tot_stats->wait_time_per_task[task] += wait_time;

    //TIME TASK
    shared_daily_stats[day].time_task_daily += task_time;
    shared_daily_stats[day].time_task_daily_per_task[task] += task_time;
    if(shared_daily_stats[day].task_done_per_task[task] != 0)shared_daily_stats[day].avg_time_tasks_done_daily_per_task[task] = (float)shared_daily_stats[day].time_task_daily_per_task[task] / shared_daily_stats[day].task_done_per_task[task];

    shared_tot_stats->task_time += task_time;
    if(shared_tot_stats->num_task_done != 0)shared_tot_stats->avg_time_task = (float)shared_tot_stats->avg_time_task / shared_tot_stats->num_task_done;
    shared_tot_stats->task_time_per_task[task] += task_time;
    if(num != 0)shared_tot_stats->avg_time_task_per_task[task] = (float)shared_tot_stats->task_time_per_task[task] / num;

    shared_daily_stats[day].avg_time_users_wait_daily = (float)shared_daily_stats[day].daily_waiting_time / shared_macros[0];
    if(shared_daily_stats[day].task_done != 0)shared_daily_stats[day].avg_time_tasks_done_daily = (float)shared_daily_stats[day].time_task_daily / shared_daily_stats[day].task_done;
    
    if(pause == true){
        shared_daily_stats[day].num_pause_daily++;
        shared_tot_stats->num_pause++;

        shared_daily_stats[day].avg_time_users_wait_daily_per_task[task] = shared_daily_stats[day].time_wait_daily_per_task[task] / num;
        shared_tot_stats->avg_time_wait_per_task[task] =  shared_tot_stats->wait_time_per_task[task] / num;

    }

    if(num != 0){
        shared_daily_stats[day].avg_num_users_daily_per_task[task] = (float)shared_daily_stats[day].user_served_per_task[task] / num;
        shared_daily_stats[day].avg_num_tasks_done_daily_per_task[task] = (float)shared_daily_stats[day].task_done_per_task[task] / num;
        shared_daily_stats[day].avg_num_tasks_not_done_daily_per_task[task] = (float)shared_daily_stats[day].task_not_done_per_task[task] / num;
    }

    shared_daily_stats[day].num_workers_active_daily++;
    shared_tot_stats->num_worker_active++;
    if(num != 0)shared_daily_stats[day].avg_num_pause_daily = (float)shared_daily_stats[day].num_pause_daily / num;

    for(int i = 0; i < shared_macros[1]; i++){
    int current_task = shared_seats[i].task;
    int workers_for_task = shared_tot_stats->num_worker_per_task[current_task];
    shared_tot_stats->num_ratio_worker_user[i] = ratio_worker_seats(current_task, workers_for_task);
    }
}

float ratio_worker_seats(int task, int num){
    
    int num_workerseats_per_task = 0;
    for(int i = 0; i < shared_macros[1]; i++){
        if(shared_seats[i].task == task){
            num_workerseats_per_task++;
        }
    }

    if(num_workerseats_per_task != 0)return (float)num / num_workerseats_per_task;
    return 0;
}
