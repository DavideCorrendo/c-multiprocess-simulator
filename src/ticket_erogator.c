#include "main.h"

int main(){
    struct message msg;

    enum tasks task;

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

    int semid = semget(SEM_KEY, shared_macros[1] + 5, 0);
    if(semid == -1) {
        perror("semget");
        exit(1);
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