#include "main.h"

int main(){

    struct message msg;

    const char *file_timeout = "config_timeout.conf";
    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");

    int NUM_WORKERSEATS;
    int NUM_WORKER;

    enum tasks task;
    int random = rand() % 7;
    task = (enum tasks)random;

    int msgid = msgget(MSG_KEY, 0);

    msgrcv(msgid, &msg, sizeof(msg.MACRO), 1, 0);
    NUM_WORKER = msg.MACRO; 
    msgrcv(msgid, &msg, sizeof(msg.MACRO), 1, 0);
    NUM_WORKERSEATS = msg.MACRO; 

    int semid = semget(SEM_KEY, 3 * NUM_WORKERSEATS, 0);
    initSem(semid, 0);

    int shmid_stats;
    int shmid_seats;

    shmid_stats = shmget(SHM_KEY_STATS, sizeof(stats) * SIM_DURATION, 0);
    shmid_seats = shmget(SHM_KEY_SEATS, NUM_WORKERSEATS * sizeof(worker_seat), 0);

    stats *shared_stats = shmat(shmid_stats, NULL, 0);
    worker_seat *shared_seats = shmat(shmid_seats, NULL, 0);

    



    return 1;
}