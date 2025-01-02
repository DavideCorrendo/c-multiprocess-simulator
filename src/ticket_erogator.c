#include "main.h"

static int *shared_macros = NULL;
static worker_seat *shared_seats = NULL;

void inizialize_keys_modified(key_t  *shm_macros_key,key_t  *sem_key, key_t *msg_key, key_t *shm_seats_key);

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
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction SIGHUP");
        exit(EXIT_FAILURE);
    }

    struct message msg;

    int task;

    key_t shm_macros_key;
    key_t shm_seats_key;
    key_t sem_key;
    key_t msg_key;

    inizialize_keys_modified(&shm_macros_key, &sem_key, &msg_key, &shm_seats_key);
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
    shared_macros = shmat(shmid_macros, NULL, 0);

    int semid = semget(sem_key, 3 * shared_macros[1] + 2, 0);
    if(semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    int shmid_seats = shmget(shm_seats_key, shared_macros[1] * sizeof(worker_seat), 0);
    shared_seats = shmat(shmid_seats, NULL, 0);

    while(msg.mtext != "end_simulation"){

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

void inizialize_keys_modified(key_t  *shm_macros_key,key_t  *sem_key,key_t *msg_key, key_t *shm_seats_key) {
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
    if((*shm_seats_key = ftok("/tmp", 'C')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
}
