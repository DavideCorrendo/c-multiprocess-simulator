#include "main.h"

static int *shared_macros = NULL;
static worker_seat *shared_seats = NULL;

void office_time(struct message msg, int task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats, bool *end_day);
void task_time(struct message msg, int msgid, int semid, int *shared_macros, int worker_id, bool *end_day);
void cleanup_resources();

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

    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys_modified(&shm_macros_key, &sem_key, &msg_key, &shm_seats_key);
    int msgid = msgget(msg_key, 0);
    if(msgid == -1) {
        perror("msgget in user");
        raise(SIGTERM);
    }

    int shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1) {
        perror("shmget in user for macros");
        raise(SIGTERM);
    }
    shared_macros = shmat(shmid_macros, NULL, 0);
    if (shared_macros == (void *)-1) {
        perror("shmat failed for macros");
        raise(SIGTERM);
    }

    int semid = semget(sem_key, 3 * shared_macros[1] + 2, 0);
    if(semid == -1) {
        perror("semget");
        raise(SIGTERM);
    }

    int shmid_seats = shmget(shm_seats_key, shared_macros[1] * sizeof(worker_seat), 0);
    if(shmid_seats == -1) {
        perror("shmget in user for seats");
        raise(SIGTERM);
    }
    shared_seats = shmat(shmid_seats, NULL, 0);
    if (shared_seats == (void *)-1) {
        perror("shmat failed for seats");
        raise(SIGTERM);
    }


    bool end_day;
    sleep(1);
    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0);

    //<-------------------------SEMAPHORE FOR THE TICKETS EROGATOR - USERS COMUNICATION GESTION-------------------------------->


        for(int i = 0; i < shared_macros[3] && strcmp(msg.mtext, "end_simulation") != 0; i++) {
            int P_SERV = rand() % (P_SERV_MAX - P_SERV_MIN + 1) + P_SERV_MIN;//probabilty to go to the office

            int decision = rand() % 101;
            int num_task = (rand() % 5) + 1;
            int time = rand() % 480;
            end_day = false;

            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0);
            usleep(time * N_NANO_SEC);

            while(decision <= P_SERV && num_task != 0 && !end_day){
                //<----------------TIME DECISION---------------------------->
                int task = rand() % 6;

                office_time(msg, task, msgid, semid, shared_macros, shared_seats, &end_day);
                if(end_day)break;

                num_task--;
            }
            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 5, IPC_NOWAIT);
        }
    
    cleanup_resources(shared_macros, shared_seats);
    return 1;
}
void office_time(struct message msg, int task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats, bool *end_day) {
    msg.mtype = 4;
    msg.num = task;
    wait_semaphore(semid, shared_macros[1] + 2);
    msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);
    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 4, 0);
    signal_semaphore(semid, shared_macros[1] + 2);
    if(msg.num == -1) {
        return;
    }
    int seat_num = msg.num;

    task_time(msg, msgid, semid, shared_macros, shared_seats[seat_num].worker_id, end_day);

}

void task_time(struct message msg, int msgid, int semid, int *shared_macros, int worker_id, bool *end_day) {
    int start_time = shared_macros[2];
    wait_semaphore(semid, shared_macros[1]- 1);
    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 0, IPC_NOWAIT);
    if(strcmp(msg.mtext, "end") == 0){
        *end_day = true;
        signal_semaphore(semid, shared_macros[1]- 1);
        return;
    }

    msg.mtype = 5 + worker_id;
    msg.num = shared_macros[2] - start_time;
    msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);
    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long),  5 + worker_id, 0);

    signal_semaphore(semid, shared_macros[1]- 1);
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