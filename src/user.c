#include "main.h"

static int *shared_macros = NULL;
static worker_seat *shared_seats = NULL;

void office_time(struct message msg, int task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats, bool *end_day);
void task_time(struct message msg, int msgid, int semid, int *shared_macros, int worker_id, int seat_num, bool *end_day);
void cleanup_resources();
int random_weighted(int values[], int weights[], int size);

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

    srand(time(NULL) + getpid());

    //int values[] = {0, 1, 2, 3, 4, 5};
    //int weights[] = {50, 20, 15, 10, 4, 1};
    //int size_r = sizeof(values) / sizeof(values[0]);


    //<-------------------------SEMAPHORE FOR THE TICKETS EROGATOR - USERS COMUNICATION GESTION-------------------------------->


        while(strcmp(msg.mtext, "end_simulation") != 0) {

            //printf("%d iniziato \n", getpid());

            int P_SERV = rand() % (P_SERV_MAX - P_SERV_MIN + 1) + P_SERV_MIN;//probabilty to go to the office
            
            int decision = rand() % 101;
            int num_task = (rand() % shared_macros[6]) + 1;
            int time = rand() % 480;
            end_day = false;
            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0);
            usleep((time * N_NANO_SEC) / 1000);
            

            while(decision <= P_SERV && !end_day && num_task > 0){

                int task = rand() % 6;
                //printf("[user] task decisa %d\n", task);

                office_time(msg, task, msgid, semid, shared_macros, shared_seats, &end_day);
                if(end_day)break;

                msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 2, IPC_NOWAIT);
                if(strcmp(msg.mtext, "end") == 0)break;

                num_task--;
            }

            puts("user finito");
            wait_semaphore(semid, shared_macros[1] + 1);
            shared_macros[5]++;
            signal_semaphore(semid, shared_macros[1] + 1);
            wait_semaphore(semid, shared_macros[1] + 3);
            msg.mtext[0] = '\0';

            msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 5, 0);

            change_msg(&msgid);
            printf("nuovo id %d\n", msgid);
        }
    
    cleanup_resources(shared_macros, shared_seats);
    return 1;
}
void office_time(struct message msg, int task, int msgid, int semid, int *shared_macros, worker_seat *shared_seats, bool *end_day) {
    msg.mtype = 4;
    msg.num = task;
    wait_semaphore(semid, shared_macros[1] + 2);
    //printf("[user] preso controllo del ticket erogator %d\n", msg.num);
    msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);
    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 4, 0);
    //printf("ticket ricevuto %d\n", msg.num);
    signal_semaphore(semid, shared_macros[1] + 2);
    if(msg.num == -1) {
        *end_day = true;
        return;
    }
    int seat_num = msg.num;

    task_time(msg, msgid, semid, shared_macros, shared_seats[seat_num].worker_id, seat_num, end_day);

}

void task_time(struct message msg, int msgid, int semid, int *shared_macros, int worker_id, int seat_num, bool *end_day) {
    int start_time = shared_macros[2];
    wait_semaphore(semid, seat_num);
    msg.mtype = 5 + worker_id;
    msg.num = shared_macros[2] - start_time;

    msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long), 2, IPC_NOWAIT);
    if(strcmp(msg.mtext, "end") == 0){
        *end_day = true;
        msg.num = -1;
    }
    
    msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);
    if(*end_day)msgrcv(msgid, &msg, sizeof(struct message) - sizeof(long),  5 + worker_id, 0);

    signal_semaphore(semid, seat_num);
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

int random_weighted(int values[], int weights[], int size) {

    int total_weight = 0;
    for (int i = 0; i < size; i++) {
        total_weight += weights[i];
    }

    int rand_num = rand() % total_weight;

    for (int i = 0; i < size; i++) {
        if (rand_num < weights[i]) {
            return values[i];
        }
        rand_num -= weights[i];
    }

    return -1;
}