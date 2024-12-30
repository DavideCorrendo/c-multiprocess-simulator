#include "main.h"


int main(){

    struct message msg;

    const char *file_timeout = "config_timeout.conf";
    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");

    int id_worker;
    int shmid_stats;
    int shmid_seats;
    int shmid_macros;
    int msgid;
    int semid;

    int *shared_macros;
    worker_seat *shared_seats;
    stats *shared_stats;

    srand(time(NULL));
    enum tasks task;
    int random = (rand() % 6) + 1;
    task = (enum tasks)random;

    get_function(&msgid, &shmid_macros, shmid_stats, &shmid_seats, &semid, shared_macros, shared_stats, shared_seats);

    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 2, 0);
    id_worker = msg.num;

    int *shared_macros = shmat(shmid_macros, NULL, 0);
    stats *shared_stats = shmat(shmid_stats, NULL, 0);
    worker_seat *shared_seats = shmat(shmid_seats, NULL, 0);

    

    int time_tasks[6] = TIMES_ARRAY;
    int pause_counter = 0;
    
    for(int i = 1; i <= shared_macros[3] && strcmp(msg.mtext, "end") != 0; i++) {  

        wait_semaphore(semid, shared_macros[1]);
        shared_stats[i].num_ratio_worker_user[task]++;
        signal_semaphore(semid, shared_macros[1]);

        while(!find_seat(&shared_seats, shared_macros, task, id_worker, semid)){usleep(15000);};

        working_time(&shared_seats, &shared_stats, shared_macros, task, time_tasks[(int)task - 1], 
        id_worker, &msg, msgid, semid, pause_counter, i);  

        msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 5, IPC_NOWAIT);  

    }
    
   
    exit(EXIT_SUCCESS);
} 

void working_time(worker_seat *shared_seats, stats *shared_stats, int *shared_macros, enum tasks task, int avg_time_task, int id_worker, struct message *msg, int msgid, int semid, int pause_counter, int day) {
    
    char s[10] = "done";

    msgrcv(msgid, msg, sizeof(struct message), 1, 0);
    bool pause = false;
    int start_task;
    int end_task;
    int user_served = 0;
    int time_task_count = 0;
    int num_pause_daily = 0;

    while(strcmp(msg->mtext, "end") != 0 && pause == false){

        msgrcv(msgid, msg, sizeof(struct message), 9 + id_worker, 0);
        start_task = shared_macros[2];

        float time_task = ((float)rand() / RAND_MAX) + 0.5;
        time_task *= avg_time_task;
        usleep(time_task * N_NANO_SEC);


        end_task = shared_macros[2];
        strcpy(msg->mtext, s);    
        msg->mtype = 9 + id_worker;
        msgsnd(msgid, msg, sizeof(struct message) - sizeof(long), 0);

        user_served++;
        time_task_count += (end_task - start_task);

        if(((rand() % 100) <= 10) && pause_counter < shared_macros[4]){
            pause_counter++;
            num_pause_daily++;    
            pause = true;
        }

        msgrcv(msgid, msg, sizeof(struct message), 1, IPC_NOWAIT);

    }

    usleep(1500);
    wait_semaphore(semid, shared_macros[1]);

    update_stats(shared_stats, semid, day, shared_macros, task, shared_seats, user_served, time_task_count);

    signal_semaphore(semid, shared_macros[1]);

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

void update_stats(stats *shared_stats, int semid, int day, int *shared_macros, enum tasks task, worker_seat *shared_seats, int user_served, int time_task_count){

    int num = worker_per_task(shared_macros, shared_seats, task);

    shared_stats[day].prev_user_served_daily[(int)task] += user_served;
    shared_stats[day].user_served_daily += user_served;
    shared_stats[day].time_task_daily += time_task_count;
    shared_stats[day].time_task_tot += time_task_count;
    shared_stats[day].prev_time_task_daily[(int)task] += time_task_count;
    shared_stats[day].prev_time_task_tot[(int)task] += time_task_count;
    if(pause == true)shared_stats[day].num_pause_daily_tot++;
    shared_stats[day].num_workers_active_daily++;

    shared_stats[day].tot_num_users_tot += user_served;
    shared_stats[day].avg_num_users_daily = shared_stats[day].user_served_daily / shared_macros[0];
    shared_stats[day].tot_num_tasks_done += user_served;
    shared_stats[day].tot_num_tasks_not_done = shared_stats[day].user_not_served_daily;
    shared_stats[day].avg_num_tasks_done = shared_stats[day].user_served_daily / shared_macros[0];                                                           
    shared_stats[day].avg_num_tasks_not_done = shared_stats[day].user_not_served_daily / shared_macros[0];
    shared_stats[day].avg_time_users_wait_tot = shared_stats[day].tot_waiting_time / shared_stats[day].tot_num_users_tot;
    shared_stats[day].avg_time_users_wait_daily = shared_stats[day].daily_waiting_time / shared_stats[day].user_served_daily;
    shared_stats[day].avg_time_tasks_done_tot = shared_stats[day].time_task_tot / shared_stats[day].tot_num_users_tot;
    shared_stats[day].avg_time_tasks_done_daily = shared_stats[day].time_task_daily / shared_stats[day].user_served_daily;   

    
    shared_stats[day].prev_stats_tot_users[(int)task] += shared_stats[day].user_served_daily;
    shared_stats[day].prev_stats_avg_users[(int)task] = shared_stats[day].user_served_daily / num;
    shared_stats[day].prev_stats_tot_tasks_done[(int)task] = shared_stats[day].prev_stats_tot_users[task];
    shared_stats[day].prev_stats_avg_tasks_done[(int)task] = shared_stats[day].prev_stats_tot_users[task] / num;


    shared_stats[day].prev_stats_avg_wait_tot[(int)task] = shared_stats[day].prev_time_wait_tot[(int)task] / num;
    shared_stats[day].prev_stats_avg_wait_daily[(int)task] = shared_stats[day].prev_time_wait_daily[(int)task] / num;
    shared_stats[day].prev_stats_avg_done_tot[(int)task] = shared_stats[day].prev_time_task_tot[(int)task] / num;
    shared_stats[day].prev_stats_avg_done_daily[(int)task] = shared_stats[day].prev_time_task_daily[(int)task] / num;
    
    shared_stats[day].num_pause_tot += shared_stats[day].num_pause_daily_tot;
    shared_stats[day].avg_num_pause_daily = shared_stats[day].num_pause_daily_tot / num;
    shared_stats[day].num_workers_active_tot += shared_stats[day].num_workers_active_daily;

    for(int i = 0; i < shared_macros[1]; i++){
        shared_stats[day].num_ratio_worker_user[i] = shared_macros[12 + (int)task] / ratio_worker_seats(shared_seats[i].task, shared_macros, shared_seats);
    }

}



float ratio_worker_seats(enum tasks task, int *shared_macros, worker_seat *shared_seats){
    int seats_per_task = 0;

    for(int i = 0; i < shared_macros[1]; i++){
        if(shared_seats[i].task == task){
            seats_per_task++;
        }
    }
    return seats_per_task;
}

