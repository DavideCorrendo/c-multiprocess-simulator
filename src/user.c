#include "main.h"

void office_time(struct message msg, enum tasks task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats);
void task_time(struct message msg, int msgid, int semid, int *shared_macros);

int main() {
    struct message msg;

    // chose a random task
    srand(time(NULL));
    enum tasks task;
    int random = rand() % 7;
    task = (enum tasks)random;

    //initializzation
    int msgid = msgget(MSG_KEY, 0);

    int shmid_macros = shmget(SHM_KEY_MACROS, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1) {
        perror("shmget");
        exit(1);
    }
    int *shared_macros = shmat(shmid_macros, NULL, 0);

    int semid = semget(SEM_KEY, 3 * shared_macros[1] + 2, 0);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }

    int shmid_seats = shmget(SHM_KEY_SEATS, shared_macros[1] * sizeof(worker_seat), 0);
    if(shmid_seats == -1) {
        perror("shmget");
        exit(1);
    }
    worker_seat *shared_seats = shmat(shmid_seats, NULL, 0);

    //<-------------------------SEMAPHORE FOR THE TICKETS EROGATOR - USERS COMUNICATION GESTION-------------------------------->

    while(msg.mtext != "end") {
        for(int i = 0; i < shared_macros[3]; i++) {
            int P_SERV = rand() % (P_SERV_MAX - P_SERV_MIN + 1) + P_SERV_MIN;

            int decision = rand() % 101;

            if(decision <= P_SERV) {
                //<----------------TIME DECISION---------------------------->
                for(int j = 0; j < 480; j++) {      //60 * 8 = minutes for hour => time of daily work
                    if(decision >= 40) {
                        //go to the postal office
                        usleep(100);

                        office_time(msg, task, msgid, semid, shared_macros, shared_seats);
                    }
                    decision++;
                }
            }
        }
    }
    
    return 1;
}
void office_time(struct message msg, enum tasks task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats) {
    msgrcv(msgid, &msg, sizeof(struct message), 4, 0);
    //scrivi msgsnd
    if(msg.num == -1) {
        return;
    }

    int seat_num = msg.num;
    signal_semaphore(semid, shared_macros[1] + 1);

    //<--------------------RESEARCH OF THE CORRECT SEAT, WAIT IN SEM-QUEUE, AND REQUEST THE TASK----------------------->
    for(int i = 0; i < shared_macros[1]; i++) {
        if(seat_num == shared_seats[i].worker_id) {
            task_time(msg, msgid, semid, shared_macros);
        }
    }
}

void task_time(struct message msg, int msgid, int semid, int *shared_macros) {
    wait_semaphore(semid, shared_macros[1]- 1);
    msg.mtype = 10;
    msgsnd();
}