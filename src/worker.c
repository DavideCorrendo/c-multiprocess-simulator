#include "main.h"

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
    // Detach from shared memory segments
    if (shared_daily_stats != NULL) {
        shmdt(shared_daily_stats);
        shared_daily_stats = NULL;
    }
    if (shared_tot_stats != NULL) {
        shmdt(shared_tot_stats);
        shared_tot_stats = NULL;
    }
    if (shared_seats != NULL) {
        shmdt(shared_seats);
        shared_seats = NULL;
    }
    if (shared_macros != NULL) {
        shmdt(shared_macros);
        shared_macros = NULL;
    }
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
}

// Signal handler
void signal_handler(int signum) {
    cleanup_resources();
    exit(signum);
}

int main(int argc, char *argv[]) {

    struct message msg;
    const char *file_timeout = "config_timeout.conf";
    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");

    int id_worker = atoi(argv[1]);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    srand(time(NULL));
    int task = rand() % 6;

    
    key_t shm_daily_stat_key;
    key_t shm_tot_stat_key;
    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys(&shm_daily_stat_key, &shm_tot_stat_key, &shm_seats_key, &shm_macros_key, &sem_key, &msg_key);

    shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0);
    shared_macros = shmat(shmid_macros, NULL, 0);
    if (shared_macros == (void *)-1) {
        perror("shmat failed for macros");
        exit(EXIT_FAILURE);
    }

    shmid_daily_stats = shmget(shm_daily_stat_key, shared_macros[3] * sizeof(daily_stats), 0);
    shared_daily_stats = shmat(shmid_daily_stats, NULL, 0);
    if (shared_daily_stats == (void *)-1) {
        perror("shmat failed for stats");
        exit(EXIT_FAILURE);
    }

    shmid_tot_stats = shmget(shm_tot_stat_key, shared_macros[3] * sizeof(daily_stats), 0);
    shared_tot_stats = shmat(shmid_tot_stats, NULL, 0);
    if (shared_tot_stats == (void *)-1) {
        perror("shmat failed for stats");
        exit(EXIT_FAILURE);
    }

    shmid_seats = shmget(shm_seats_key, shared_macros[1], 0);
    shared_seats = shmat(shmid_seats, NULL, 0);
    if (shared_seats == (void *)-1) {
        perror("shmat failed for seats");
        exit(EXIT_FAILURE);
    }

    int time_tasks[6] = TIMES_ARRAY;
    int pause_counter = 0;
    memset(&msg, 0, sizeof(msg));

    for (int i = 1; i <= shared_macros[3] && strcmp(msg.mtext, "end_simulation") != 0; i++) {

        while (!find_seat(shared_seats, shared_macros, task, id_worker, semid)) {
            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, IPC_NOWAIT);
            if(strcmp(msg.mtext, "end") == 0)continue;
            usleep(500);
        }

        working_time(shared_seats, shared_daily_stats, shared_tot_stats, shared_macros, task, time_tasks[task], id_worker, &msg, msgid, semid, pause_counter, i);
        msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 5, IPC_NOWAIT);
    }

    cleanup_resources();
    exit(EXIT_SUCCESS);
}

void working_time(worker_seat *shared_seats, daily_stats *shared_daily_stats, tot_stats *shared_tot_stats, int *shared_macros, int task, int avg_time_task, int id_worker, struct message *msg, int msgid, int semid, int pause_counter, int day) {
    char s[10] = "done";

    msgrcv(msgid, msg, sizeof(struct message), 1, 0);
    bool pause = false;
    int start_task, end_task, user_served = 0, time_task_count = 0, wait_time = 0;

    while (strcmp(msg->mtext, "end") != 0 && !pause) {
        msgrcv(msgid, msg, sizeof(struct message), 5 + id_worker, 0);
        wait_time += msg->num;
        start_task = shared_macros[2];

        float time_task = ((float)rand() / RAND_MAX) + 0.5;
        time_task *= avg_time_task;
        usleep(time_task * N_NANO_SEC);

        end_task = shared_macros[2];
        strcpy(msg->mtext, s);
        msg->mtype = 5 + id_worker;
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);

        user_served++;
        time_task_count += (end_task - start_task);

        if (((rand() % 100) <= 10) && pause_counter < shared_macros[4]) {
            pause_counter++;
            pause = true;
        }

        msgrcv(msgid, msg, sizeof(struct message), 1, IPC_NOWAIT);
    }

    usleep(500);
    wait_semaphore(semid, shared_macros[1]);
    update_stats(shared_daily_stats, shared_tot_stats, semid, day, shared_macros, task, shared_seats, user_served, time_task_count, pause, wait_time);
    signal_semaphore(semid, shared_macros[1]);
}

bool find_seat(worker_seat *shared_seats, int *shared_macros, int task, int id_worker, int semid) {
    for (int i = 0; i < shared_macros[1]; i++) {
        wait_semaphore(semid, i);
        if (shared_seats[i].task == task && !shared_seats[i].busy) {
            if (!shared_seats[i].busy) {
                shared_seats[i].busy = true;
                shared_seats[i].worker_id = id_worker;
                signal_semaphore(semid, i);
                return true;
            }
        }
        signal_semaphore(semid, i);
    }
    return false;
}

void update_stats(daily_stats *shared_daily_stats, tot_stats *shared_tot_stats, int semid, int day, int *shared_macros, int task, worker_seat *shared_seats, int user_served, int task_time, bool pause, int wait_time){

    int num = worker_per_task(shared_macros, shared_seats, task);

    shared_tot_stats->wait_time += wait_time;
    shared_tot_stats->task_time += task_time;

    shared_tot_stats->wait_time_per_task[task] += wait_time;
    shared_tot_stats->task_time_per_task[task] += task_time;

    shared_tot_stats->num_user_served += user_served;
    shared_tot_stats->num_task_done += user_served; 
    shared_tot_stats->avg_time_task = shared_tot_stats->task_time / shared_tot_stats->num_user_served;
    shared_tot_stats->avg_time_wait = shared_tot_stats->wait_time / shared_tot_stats->num_user_served;

    shared_tot_stats->avg_time_wait_per_task[task] = shared_tot_stats->wait_time_per_task[task] / shared_tot_stats->num_user_served_per_task[task];
    shared_tot_stats->avg_time_task_per_task[task] = shared_tot_stats->task_time_per_task[task] / shared_tot_stats->num_user_served_per_task[task];

    shared_tot_stats->num_user_served_per_task[task] += user_served;
    shared_tot_stats->num_task_done_per_task[task] += user_served; 

    shared_daily_stats[day].user_not_served_daily += user_served;
    shared_daily_stats[day].daily_waiting_time += wait_time;
    shared_daily_stats[day].time_task_daily += task_time;

    shared_daily_stats[day].time_task_daily_per_task[task] += task_time;
    shared_daily_stats[day].time_wait_daily_per_task[task] += wait_time;

    shared_daily_stats[day].avg_num_users_daily = shared_daily_stats[day].user_not_served_daily / shared_macros[0];
    shared_daily_stats[day].avg_num_tasks_done_daily = shared_daily_stats[day].user_not_served_daily / shared_macros[0];

    shared_daily_stats[day].avg_time_users_wait_daily = shared_daily_stats[day].daily_waiting_time / shared_macros[0];
    shared_daily_stats[day].avg_time_tasks_done_daily = shared_daily_stats[day].time_task_daily / shared_macros[0];


    int num_task_not_done = num_user_waiting(semid, shared_macros);

    shared_tot_stats->num_task_not_done += num_task_not_done;

    shared_daily_stats[day].user_served_per_task[task] += user_served;
    shared_daily_stats[day].user_not_served_per_task[task] ;
    shared_daily_stats[day].user_not_served_daily += user_served; 
    shared_daily_stats[day].avg_num_tasks_not_done_daily = num_task_not_done / shared_macros[0]; 

    shared_daily_stats[day].avg_num_users_daily_per_task[task] += user_served / num;
    shared_daily_stats[day].avg_num_tasks_done_daily_per_task[task] += user_served / num;
    shared_daily_stats[day].avg_num_tasks_not_done_daily_per_task[task] += num_task_not_done / num;
    shared_daily_stats[day].avg_time_users_wait_daily_per_task[task] = shared_daily_stats[day].time_wait_daily_per_task[task] / num;
    shared_daily_stats[day].avg_time_tasks_done_daily_per_task[task] += shared_daily_stats[day].time_task_daily_per_task[task] / num;

    if(pause == true){
        shared_daily_stats[day].num_pause_daily++;
        shared_tot_stats->num_pause++;
    }

    shared_daily_stats[day].num_workers_active_daily++;
    shared_tot_stats->num_worker_active++;
    shared_daily_stats[day].avg_num_pause_daily = shared_daily_stats[day].num_pause_daily / num;

    for(int i = 0; i < shared_macros[1]; i++){
        shared_daily_stats[day].num_ratio_worker_user[i] = ratio_worker_seats(shared_macros, shared_seats, i);
    }

}

float ratio_worker_seats(int *shared_macros, worker_seat *shared_seats, int index){
    
    int num_workerseats_per_task = 0;
    for(int i = 0; i < shared_macros[1]; i++){
        if(shared_seats[index].task == shared_seats[i].task){
            num_workerseats_per_task++;
        }
    }
    return worker_per_task(shared_macros, shared_seats, shared_seats[index].task) / num_workerseats_per_task;
}

int worker_per_task(int* shared_macros, worker_seat *shared_seats, int task){
    int cont = 0;
    for(int i = 0; i < shared_macros[0]; i++){
        if(shared_seats[i].task == task){
            cont++;
        }
    }
    return cont;
}

