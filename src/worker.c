#include "main.h"

int main(){

    struct message msg;

    const char *file_timeout = "config_timeout.conf";
    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");

    int id_worker;

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

    bool sentinel_conditions = false;
    for(int i = 0; i < shared_macros[1] && !sentinel_conditions; i++) {
        if(task == shared_seats[i].task) {
            if(shared_seats[i].busy == false) {
                shared_seats[i].busy = true;
                shared_seats[i].worker_id = id_worker;
                sentinel_conditions = true;
            }
        }
    }

    msg.mtype = /*?*/;

    while( /*WE ALREADY MUST DEFINE THE CONDITION FOR EXIT TO THE WAIT CICLE*/) {     /*<------------IT MUST BE MODIFIED BECAUSE IT'S AN ACTIVE WAIT*/
        if(msgrcv(shared_seats->id, &msg, sizeof(struct message), /*?*/) == -1) {
            perror("msgrcv");
            exit(1);
        }

        
    }

    return 1;
}