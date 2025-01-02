#include "main.h"

void office_time(struct message msg, enum tasks task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats);
void task_time(struct message msg, int msgid, int semid, int *shared_macros, int task);
void inizialize_keys_modified(key_t *shm_seats_key,key_t  *shm_macros_key,key_t  *sem_key,key_t *msg_key);

int main() {
    struct message msg;

    //initializzation

    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    inizialize_keys_modified(&shm_seats_key, &shm_macros_key, &sem_key, &msg_key);
    int msgid = msgget(msg_key, 0);
    if(msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    int shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    int *shared_macros = shmat(shmid_macros, NULL, 0);

    int semid = semget(sem_key, 3 * shared_macros[1] + 2, 0);
    if(semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    int shmid_seats = shmget(shm_seats_key, shared_macros[1] * sizeof(worker_seat), 0);
    if(shmid_seats == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }
    worker_seat *shared_seats = shmat(shmid_seats, NULL, 0);

    //<-------------------------SEMAPHORE FOR THE TICKETS EROGATOR - USERS COMUNICATION GESTION-------------------------------->

    while(msg.mtext != "end") {
        for(int i = 0; i < shared_macros[3]; i++) {
            int P_SERV = rand() % (P_SERV_MAX - P_SERV_MIN + 1) + P_SERV_MIN;

            int decision = rand() % 101;

            if(decision <= P_SERV) {
                //<----------------TIME DECISION---------------------------->
                int P_MULTI_TASK = rand() % 6;      //multi_task probability

                for(int j = 0; j < 480; j++) {      //60 * 8 = minutes for hour => time of daily work
                    if(decision >= 40) {
                        // chose a random tasks
                        srand(time(NULL));
                        int task = rand() % 6;

                        //go to the postal office
                        usleep(100);

                        office_time(msg, task, msgid, semid, shared_macros, shared_seats);
                    }
                    decision += 2;
                }
            }
        }
    }
    
    return 1;
}
void office_time(struct message msg, int task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats) {
    msg.mtype = 4;
    msgsnd(msgid, &msg, sizeof(struct message), 0);
    msgrcv(msgid, &msg, sizeof(struct message), 4, 0);
    if(msg.num == -1) {
        return;
    }

    int seat_num = msg.num;
    signal_semaphore(semid, shared_macros[1] + 1);

    //<--------------------RESEARCH OF THE CORRECT SEAT, WAIT IN SEM-QUEUE, AND REQUEST THE TASK----------------------->
    for(int i = 0; i < shared_macros[1]; i++) {
        if(seat_num == shared_seats[i].worker_id) {
            task_time(msg, msgid, semid, shared_macros, task);
        }
    }
}

void task_time(struct message msg, int msgid, int semid, int *shared_macros, int task) {
    wait_semaphore(semid, shared_macros[1]- 1);

    msg.mtype = 10;
    msgsnd(msgid, &msg, sizeof(struct message), 0);

    msgrcv(msgid, &msg, sizeof(struct message), 10, 0);

    task = 10;
}

void inizialize_keys_modified(key_t *shm_seats_key,key_t  *shm_macros_key,key_t  *sem_key,key_t *msg_key) {
    if((*shm_seats_key = ftok("/tmp", 'C')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*shm_macros_key = ftok("/tmp", 'D')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*sem_key = ftok("/tmp", 'E')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*msg_key = ftok("/tmp", 'F')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
}