#include "../include/main.h"

static shared_data *shared_macros = NULL;
static worker_seat *shared_seats = NULL;

void cleanup_resources();
int search_seat(int task, worker_seat *shared_seats, int semid, int *seat_visits);

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

    //printf("[%d] ticket er. iniziato \n", getpid());

    struct message msg;

    key_t shm_data_key;
    key_t shm_seats_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys_modified(&shm_data_key, &sem_key, &msg_key, &shm_seats_key);
    int msgid = msgget(msg_key, 0);
    if(msgid == -1) {
        perror("msgget in ticket");
        raise(SIGTERM);
    }

    int shmid_data = shmget(shm_data_key, sizeof(struct shared_data), 0);    //<-----------------da guardare sostituito con NUM_MACROS-------------->
    if(shmid_data == -1) {
        perror("shmget in ticket erogator");
        raise(SIGTERM);
    }
    shared_macros = shmat(shmid_data, NULL, 0);

    int semid = semget(sem_key, shared_macros->NOF_WORKERSEATS + num_sem, 0);
    if(semid == -1) {
        perror("semget");
        raise(SIGTERM);
    }

    int shmid_seats = shmget(shm_seats_key, shared_macros->NOF_WORKERSEATS * sizeof(worker_seat), 0);
    if(shmid_seats == -1){
        perror("semget");
        raise(SIGTERM);
    }
    shared_seats = shmat(shmid_seats, NULL, 0);

    int *seat_visits = calloc(shared_macros->NOF_WORKERSEATS, sizeof(int));
    int avg_time_tasks[6] = TIMES_ARRAY;
    float time_task;

    srand((time(NULL)) + getpid());

    while(get_semaphore_value(semid, 3) == 0){

        //puts("qui");
        wait_signal(semid, 0);

        for (int i = 0; i < shared_macros->NOF_WORKERSEATS; i++) {
            seat_visits[i] = 0;
        }

        while(get_semaphore_value(semid, 1) == 0){
            //puts("qui t.e.");
            msg.num = 0;
            //puts("ASPETTO");
            msgrcv_wait(msgid, &msg, sizeof(struct message) - sizeof(long), 1, semid);
            //printf("[ticket er.] preso task %d\n", msg.num);
            time_task = 0.5 + (float)rand() / RAND_MAX;
            msg.time = time_task * avg_time_tasks[msg.num];

            if(get_semaphore_value(semid, 1) == 1)break;
            //puts("cerco sedia");
            msg.num = search_seat(msg.num, shared_seats, semid, seat_visits);
            //printf("trovata sedia %d \n", msg.num);
            //if(msg.num != -1)printf("[tick. er.] OCCUPATO E %d\n", shared_seats[msg.num].busy);
            //printf("EROGATOR MANDA %d\n", msg.num);
            msg.mtype = 2;
            msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);
        }

        wait_semaphore(semid, 5);
        shared_macros->processes_finished++;
        signal_semaphore(semid, 5);
        puts("ticket finito");

    }

    reset_signals_to_default();
    cleanup_resources(shared_macros, shared_seats);
    sleep(10);
    return EXIT_SUCCESS;

}

int search_seat(int task, worker_seat *shared_seats, int semid, int seat_visits[]){
    int min_users = INT_MAX;
    int min_index = -2;

    wait_semaphore(semid, 6); 
    for (int i = task; i < shared_macros->NOF_WORKERSEATS; i = i + 6) {
        if (shared_seats[i].busy && seat_visits[i] < min_users) {
            min_users = seat_visits[i];
            min_index = i;
        }
    }
    signal_semaphore(semid, 6);

    seat_visits[min_index]++;
    return min_index;
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

