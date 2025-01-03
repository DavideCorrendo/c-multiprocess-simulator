#include "main.h"

static int *shared_macros = NULL;
static worker_seat *shared_seats = NULL;

void cleanup_resources();
int search_seat(int task, worker_seat *shared_seats, int *shared_macros, int semid);

void signal_handler(int sig) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL; 
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    
    cleanup_resources();
    
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

    key_t shm_macros_key;
    key_t shm_seats_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys_modified(&shm_macros_key, &sem_key, &msg_key, &shm_seats_key);
    printf("%d\n", msg_key);
    int msgid = msgget(msg_key, 0);
    if(msgid == -1) {
        perror("msgget in ticket");
        raise(SIGTERM);
    }

    int shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1) {
        perror("shmget in ticket erogator");
        raise(SIGTERM);
    }
    shared_macros = shmat(shmid_macros, NULL, 0);

    int semid = semget(sem_key, shared_macros[1] + 3, 0);
    if(semid == -1) {
        perror("semget");
        raise(SIGTERM);
    }

    int shmid_seats = shmget(shm_seats_key, shared_macros[1] * sizeof(worker_seat), 0);
    if(shmid_seats == -1){
        perror("semget");
        raise(SIGTERM);
    }
    shared_seats = shmat(shmid_seats, NULL, 0);

    sleep(1);

    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0);

    while(strcmp(msg.mtext, "end_simulation") != 0){

        msgrcv(msgid, &msg, sizeof(struct message), 4, 0);
        
        msg.num = search_seat(msg.num, shared_seats, shared_macros, semid);
        msgsnd(msgid, &msg, sizeof(struct message), 4);

        msgrcv(msgid, &msg, sizeof(struct message), 5, IPC_NOWAIT);

    }

    cleanup_resources(shared_macros, shared_seats);
    return EXIT_SUCCESS;

}

int search_seat(int task, worker_seat *shared_seats, int *shared_macros, int semid){

    int index_min;
    int min = INT_MAX;

    for(int i = 0; i < shared_macros[1]; i++){
        if(shared_seats[i].task == task){
            int num = num_user_waiting(semid, shared_macros);
            if(min > num){
                index_min = i;
                min = num;
            }
        }
    }
    return index_min;
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