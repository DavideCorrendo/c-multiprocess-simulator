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
    msgrcv(msgid, &msg, sizeof(struct message), 4, 0);
    time_task = msg.num;

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
    initSem(semid, 0);

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

    int home_exception = rand() % (20 + 1);
    int home_counter = 0;
    int pause_done = 0;
    
    for(int i = 0; i < shared_macros[4]; i++) {

        while(!find_seat(&shared_seats, shared_macros, task, id_worker, semid)){sleep(0.001);};

        working_time(&shared_seats, &shared_stats, shared_macros, task, time_task, id_worker, msg, msgid, semid);    

    }
    
   
    return 1;
} 

void working_time(worker_seat *shared_seats, stats *shared_stats, int *shared_macros, enum tasks task, int avg_time_task, int id_worker, struct message msg, int msgid, int semid) {
    
    
    msgrcv(msgid, &msg, sizeof(struct message), 1, 0);

    while(msg.mtext != "end"){

        msgrcv(msgid, &msg, sizeof(struct message), 3, 0);

        float time_task = ((float)rand() / RAND_MAX) + 0.5;
        time_task *= avg_time_task;
        usleep(time_task * N_NANO_SEC);
        msg.mtext = "done";     //<-------UN TIPO DI MESSAGGI PER OGNI USER (PORCO DIO)(DA FARE)
        msg.mtype = 3;
        msgsnd(msgid, &msg, sizeof(struct message), 0);

        //AGGIORNA STATS

        if((rand() % 100) <= 10){
            shared_stats->
        }

        msgrcv(msgid, &msg, sizeof(struct message), 1, IPC_NOWAIT);

    }
    

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