#include "main.h"


int main(){

    struct message msg;

    const char *file_timeout = "config_timeout.conf";
    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");

    int id_worker;
    int time_task;

    srand(time(NULL));
    enum tasks task;
    int random = rand() % 7;
    task = (enum tasks)random;

    int msgid = msgget(MSG_KEY, 0);

    msgrcv(msgid, &msg, sizeof(msg.num), 2, 0);
    id_worker = msg.num;


    int shmid_macros = shmget(SHM_KEY_MACROS, sizeof(int) * 2, 0);
    if(shmid_macros == -1) {
        perror("shmget");
        exit(1);
    }
    int *shared_macros = shmat(shmid_macros, NULL, 0);

    int semid = semget(SEM_KEY, 3 * shared_macros[1], 0);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }


    int shmid_stats;
    int shmid_seats;

    shmid_stats = shmget(SHM_KEY_STATS, sizeof(stats) * SIM_DURATION, 0);
    if(shmid_stats == -1) {
        perror("shmget");
        exit(1);
    }

    shmid_seats = shmget(SHM_KEY_SEATS, shared_macros[1] * sizeof(worker_seat), 0);
    if(shmid_seats == -1) {
        perror("shmget");
        exit(1);
    }

    stats *shared_stats = shmat(shmid_stats, NULL, 0);
    worker_seat *shared_seats = shmat(shmid_seats, NULL, 0);

    

    int time_task[6] = TIMES_ARRAY;
    int pause_counter = 0;
    bool active = false;
    
    for(int i = 0; i < shared_macros[3]; i++) {  

        wait_semaphore(semid, shared_macros[1]);
        shared_stats[i].num_ratio_worker_user[task]++;
        signal_semaphore(semid, shared_macros[1]);

        while(!find_seat(&shared_seats, shared_macros, task, id_worker, semid)){sleep(0.001);};

        wait_semaphore(semid, shared_macros[1]);
        shared_stats[i].num_workers_active_daily++;
        if(active == false){shared_stats[i].num_workers_active_tot++; 
                            active = true;}
        signal_semaphore(semid, shared_macros[1]);

        working_time(&shared_seats, &shared_stats, shared_macros, task, time_task, 
        id_worker, msg, msgid, semid, pause_counter, i);    

    }
    
   
    return 1;
} 

void working_time(worker_seat *shared_seats, stats *shared_stats, int *shared_macros, enum tasks task, int avg_time_task, int id_worker, struct message msg, int msgid, int semid, int pause_counter, int day) {
    
    char s[10] = "done";

    msgrcv(msgid, &msg, sizeof(struct message), 1, 0);
    bool pause = false;
    int start_task;
    int end_task;

    while(msg.mtext != "end" && pause == false){

        msgrcv(msgid, &msg, sizeof(struct message), 9 + id_worker, 0);
        start_task = shared_macros[2];

        float time_task = ((float)rand() / RAND_MAX) + 0.5;
        time_task *= avg_time_task;
        usleep(time_task * N_NANO_SEC);
        end_task = shared_macros[2];
        strcpy(msg.mtext, s);    
        msg.mtype = 9 + id_worker;
        msgsnd(msgid, &msg, sizeof(struct message), 0);

        wait_semaphore(semid, shared_macros[1] + 2);
        shared_macros[6]++;
        shared_macros[8 + shared_macros[3] + day] += (end_task - start_task);
        signal_semaphore(semid, shared_macros[1] + 2);

        if(((rand() % 100) <= 10) && pause_counter == shared_macros[4]){
            pause_counter++;
            wait_semaphore(semid, day);

            shared_stats[day].num_pause_tot++;
            if(day > 0){
                shared_stats[day].avg_num_pause_daily = (shared_stats[day].num_pause_tot - shared_stats[day - 1].num_pause_tot) / shared_macros[0];
            }else{
                shared_stats[day].avg_num_pause_daily = shared_stats[day].num_pause_tot / shared_macros[0];
            }

            signal_semaphore(semid, day);
            pause = true;
        }

        msgrcv(msgid, &msg, sizeof(struct message), 1, IPC_NOWAIT);

    }

    wait_semaphore(semid, shared_macros[1] + 1);
    if(shared_macros[5] == false && pause == false){
        update_stats();
        shared_macros[5] = true;
    }
    signal_semaphore(semid, shared_macros[1] + 1);

}

bool find_seat(worker_seat *shared_seats, int *shared_macros, enum tasks task, int id_worker, int semid){

    bool sentinel_conditions = false;
    for(int i = 0; i < shared_macros[1] && !sentinel_conditions; i++) {
        if(task == shared_seats[i].task) {
            if(shared_seats[i].busy == false) {
                wait_semaphore(semid, i);
                shared_seats[i].busy = true;
                shared_seats[i].worker_id = id_worker;
                sentinel_conditions = true;
                signal_semaphore(semid, i);
            }
        }
    }
    return sentinel_conditions;

}

void update_stats(stats *shared_stats, int semid, int day, int num_users, int *shared_macros, enum tasks task, worker_seat *shared_seats){

    wait_semaphore(semid, shared_macros[1]);

    shared_stats[day].tot_num_users_tot = shared_macros[6];
    shared_stats[day].avg_num_users_daily = shared_macros[6] / shared_macros[0];
    shared_stats[day].tot_num_tasks_done = shared_macros[6];
    shared_stats[day].tot_num_tasks_not_done = shared_macros[7];
    shared_stats[day].avg_num_tasks_done = shared_macros[6] / shared_macros[0];                                                           
    shared_stats[day].avg_num_tasks_not_done = shared_macros[7] / shared_macros[0];
    shared_stats[day].avg_time_users_wait_tot = calculate_avg_time_wait(day, shared_macros);
    shared_stats[day].avg_time_users_wait_daily = shared_macros[8 + day] / (shared_macros[6] + shared_macros[7]);
    shared_stats[day].avg_tasks_done_tot = calculate_avg_time_task(day, shared_macros);
    shared_stats[day].avg_tasks_done_daily = shared_macros[8 + shared_macros[2] + day] / (shared_macros[6] + shared_macros[7]);                                                                                                                               
    for(int i = 0; i < shared_macros[1]; i++){
        shared_stats[day].num_ratio_worker_user[i] /= ratio_worker_seats(task, shared_macros, shared_seats);   //<----------
    }

    signal_semaphore(semid, shared_macros[1]);

}

float calculate_avg_time_wait(int day, int *shared_macros){

    float tot_wait = 0;

    for(int i = 0; i < day; i++){
        tot_wait += shared_macros[8 + i];
    }
    tot_wait = tot_wait / day;
    return tot_wait / shared_macros[0];

}

float calculate_avg_time_task(int day, int *shared_macros){

    float tot_wait = 0;

    for(int i = 0; i < day; i++){
        tot_wait += shared_macros[8 + shared_macros[2] + i];
    }
    tot_wait = tot_wait / day;
    return tot_wait / shared_macros[0];
    
}

double ratio_worker_seats(enum tasks task, int *shared_macros, worker_seat *shared_seats){
    int seats_per_task = 0;

    for(int i = 0; i < shared_macros[1]; i++){
        if(shared_seats[i].task == task){
            seats_per_task++;
        }
    }
    return seats_per_task;
}