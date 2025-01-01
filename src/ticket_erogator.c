#include "main.h"

void inizialize_keys_modified(key_t  *shm_macros_key,key_t  *sem_key,key_t *msg_key);

int main() {
    struct message msg;

    int task; //<------------------???-------------------->

    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    inizialize_keys_modified(&shm_macros_key, &sem_key, &msg_key);
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

    // msg director and ticket = 3
    // msg ticket and utente = 4

    while(msg.mtext != "end"){

        msgrcv(msgid, &msg, sizeof(struct message), 4, 0);
        char req[MAX_MSG_SIZE] = msg.mtext;

        msgsnd(msgid, &msg, sizeof(struct message), 0);

        msgrcv(msgid, &msg, sizeof(struct message), 3, IPC_NOWAIT);

        break;

    }    
}

void inizialize_keys_modified(key_t  *shm_macros_key,key_t  *sem_key,key_t *msg_key) {
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