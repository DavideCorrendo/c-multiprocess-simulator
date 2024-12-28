#include "main.h"

int main() {
    

    //<------------------GO OR NOT AT THE POSTAL OFFICE------------------------->

    struct message msg;

    // chose a random task
    srand(time(NULL));
    enum tasks task;
    int random = rand() % 7;
    task = (enum tasks)random;

    int msgid = msgget(MSG_KEY, 0);
    if(msgid == -1) {
        perror("msgget");
        exit(1);
    }

    int shmid_macros = shmget(SHM_KEY_MACROS, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1) {
        perror("shmget");
        exit(1);
    }
    int *shared_macros = shmat(shmid_macros, NULL, 0);

    //shared_macros[1] * 5 semid
    int semid = semget(SEM_KEY, shared_macros[1] * 5, 0);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }

    const char *file_timeout = "config_timeout.conf";
    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");

    int shmid_stats = shmget(SHM_KEY_STATS, sizeof(stats) * SIM_DURATION, 0);
    if(shmid_stats == -1) {
        perror("shmget");
        exit(1);
    }
    stats *shared_stats = shmat(shmid_stats, NULL, 0);
}