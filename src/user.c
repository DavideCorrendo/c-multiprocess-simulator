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

void office_time(struct message *msg, int msgid, int tasks[], int semid, int shared_macros[], int *remaining_task, int num_task, bool tasks_done[]);
void task_time(struct message msg, int msgid, int semid, int *shared_macros, int worker_id, int seat_num, bool *end_day);
void cleanup_resources();
int random_weighted(int values[], int weights[], int size);
void update_stats(int num_task, int remaining_task, int day, int *tasks, bool *tasks_done);

void signal_handler(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL; 
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    cleanup_resources(shared_macros, shared_seats);
    
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

    struct message msg;

    key_t shm_daily_stat_key;
    key_t shm_tot_stat_key;
    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys(&shm_daily_stat_key, &shm_tot_stat_key, &shm_seats_key, &shm_macros_key, &sem_key, &msg_key);

    shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1){
        perror("shmget in worker");
        raise(SIGTERM);
    }
    shared_macros = shmat(shmid_macros, NULL, 0);
    if (shared_macros == (void *)-1) {
        perror("shmat failed for macros in worker");
        raise(SIGTERM);
    }

    shmid_daily_stats = shmget(shm_daily_stat_key, shared_macros[3] * sizeof(daily_stats), 0);
    if(shmid_daily_stats == -1){
        perror("shmget in worker");
        raise(SIGTERM);
    }
    shared_daily_stats = shmat(shmid_daily_stats, NULL, 0);
    if (shared_daily_stats == (void *)-1) {
        perror("shmat failed for stats in worker");
        raise(SIGTERM);
    }

    shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), 0);
    if(shmid_tot_stats == -1){
        perror("shmget in worker");
        raise(SIGTERM);
    }
    shared_tot_stats = shmat(shmid_tot_stats, NULL, 0);
    if (shared_tot_stats == (void *)-1) {
        perror("shmat failed for stats in worker");
        raise(SIGTERM);
    }

    shmid_seats = shmget(shm_seats_key, shared_macros[1], 0);
    if(shmid_seats == -1){
        perror("shmget in worker");
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

    srand(time(NULL) + getpid());

    //int values[] = {0, 1, 2, 3, 4, 5};
    //int weights[] = {50, 20, 15, 10, 4, 1};
    //int size_r = sizeof(values) / sizeof(values[0]);
    int day = 0;


    //<-------------------------SEMAPHORE FOR THE TICKETS EROGATOR - USERS COMUNICATION GESTION-------------------------------->


        while(strcmp(msg.mtext, "end_simulation") != 0) {

            //printf("%d iniziato \n", getpid());

            int P_SERV = rand() % (P_SERV_MAX - P_SERV_MIN + 1) + P_SERV_MIN;//probabilty to go to the office
            
            int decision = rand() % 101;
            int num_task = (rand() % shared_macros[6]) + 1;
            int remaining_task = num_task;
            int *tasks = malloc(num_task * sizeof(int));
            bool *tasks_done = malloc(num_task * sizeof(bool));
            for(int i = 0; i < num_task; i++){
                tasks[i] = rand() % 6;
                tasks_done[i] = false;
            }
            //int time = rand() % 480;
            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0);
            //usleep((time * N_NANO_SEC) / 1000);
            
            if(decision <= P_SERV){
                printf("[user] num_task: %d\n", num_task);
                for(int i = 0; i < num_task; i++){
                    printf("[user] tasks[%d]: %d\n", i, tasks[i]);
                }

                office_time(&msg, msgid, tasks, semid, shared_macros, &remaining_task, num_task, tasks_done);
            }

            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 2, 0);

            wait_semaphore(semid, shared_macros[1]);
            update_stats(num_task, remaining_task, day, tasks, tasks_done);
            signal_semaphore(semid, shared_macros[1]);

            wait_semaphore(semid, shared_macros[1] + 1);
            shared_macros[5]++;
            signal_semaphore(semid, shared_macros[1] + 1);
            wait_semaphore(semid, shared_macros[1] + 3);
            msg.mtext[0] = '\0';

            day++;
            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 5, 0);

            change_msg(&msgid);
        }
    
    reset_signals_to_default();
    cleanup_resources(shared_macros, shared_seats);
    sleep(10);
    return 1;
}
void office_time(struct message *msg, int msgid, int tasks[], int semid, int shared_macros[], int *remaining_task, int num_task, bool tasks_done[]) {
    
    msg->mtype = 4; 
    int seat_num;

    for(int i = 0; i < num_task; i++){
        printf("taks[%d] = %d\n", i, tasks[i]);

        for(int j = 0; j < shared_macros[1]; j++){
            printf("%d OCCUPATO È %d\n", j, shared_seats[j].busy);
        }

        msg->num = tasks[i];
        wait_semaphore(semid, shared_macros[1] + 2);
        printf("[user %d] qui\n", getpid());
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);
        printf("[user %d] messaggio mandato\n", getpid());
        msgrcv(msgid, msg, sizeof(struct message) - sizeof(long), 4, 0);
        printf("[user %d] qua\n", getpid());
        signal_semaphore(semid, shared_macros[1] + 2);
        if(msg->num == -1){
            printf("[user %d] ricevuto -1 aspettando %d\n", getpid(), tasks[i]);
            continue;
        }
        
        seat_num = msg->num;
        msg->mtype = 6 + seat_num;
        msg->num = tasks[i];
        printf("[user %d] RICEVUTA SEDIA %d\n", getpid(), seat_num);
        
        wait_semaphore(semid, seat_num);
        printf("[user %d] quo\n", getpid());
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);
        msgrcv(msgid, msg, sizeof(struct message) - sizeof(long), 6 + seat_num, 0);
        printf("[user %d] que\n", getpid());
        //printf("[user] mandato num %d a sedia %d\n", msg->num, seat_num);
        signal_semaphore(semid, seat_num);

        if(strcmp(msg->mtext, "end") == 0)return;
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
        shared_daily_stats[day].task_done += (num_task - remaining_task);
        shared_daily_stats[day].task_not_done += remaining_task;

        shared_tot_stats->num_user_served++;
        shared_tot_stats->num_task_done += (num_task - remaining_task);
        shared_tot_stats->num_task_not_done += remaining_task;
    }

    if(shared_daily_stats[day].user_served_daily > 0){
        shared_daily_stats[day].avg_num_users_daily = shared_daily_stats[day].user_served_daily / shared_macros[0];
        shared_daily_stats[day].avg_num_tasks_done_daily = shared_daily_stats[day].task_done / shared_macros[0];
        shared_daily_stats[day].avg_num_tasks_not_done_daily = shared_daily_stats[day].task_not_done / shared_macros[0];
    }

    for(int i = 0; i < num_task; i++){
        if(tasks_done[i]){
            shared_daily_stats[day].user_served_per_task[tasks[i]]++;
            shared_daily_stats[day].task_done_per_task[tasks[i]]++;

            shared_tot_stats->num_user_served_per_task[tasks[i]]++;
            shared_tot_stats->num_task_done_per_task[tasks[i]]++;
        }else{
            shared_daily_stats[day].task_not_done_per_task[tasks[i]]++;
            shared_tot_stats->num_task_not_done_per_task[tasks[i]]++;
        }
    }

}
