#include "../include/main.h"

union semun arg;

int semop_retry(int semid, struct sembuf *sops, size_t nsops) {
    int ret;
    do {
        ret = semop(semid, sops, nsops);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

int sem_operation(int semid, int sem_num, int op_value) {
    struct sembuf sem_op;
    
    sem_op.sem_num = sem_num;
    sem_op.sem_op = op_value;
    sem_op.sem_flg = 0;

    if (semop_retry(semid, &sem_op, 1) == -1) {
        perror("semop failed");
        raise(SIGTERM);
    }

    return 0;
}

int wait_semaphore(int semid, int sem_num) {
    return sem_operation(semid, sem_num, -1);
}

int signal_semaphore(int semid, int sem_num) {
    return sem_operation(semid, sem_num, 1);
}

void wait_signal(int semid, int sem_num){
    while(get_semaphore_value(semid,sem_num) == 0 ){
        usleep(750);
    }
}

int init_semaphore(int semid, int sem_num, int value) {
    arg.val = value;
    if (semctl(semid, sem_num, SETVAL, arg) == -1) {
        perror("semctl SETVAL failed");
        return -1;
    }
    return 0;
}

int get_semaphore_value(int semid, int sem_num) {
    int val = semctl(semid, sem_num, GETVAL);
    if (val == -1) {
        perror("semctl GETVAL failed");
    }
    return val;
}

void initialize_IPC(int* msgid_ticket, int **msgid, int *semid, daily_stats **shared_daily_stats, tot_stats **shared_tot_stats, worker_seat **shared_seats, shared_data **shared_macros){    
    key_t shm_daily_stat_key = ftok(FTOK_PATH, FTOK_DAILY_STATS);
    key_t shm_tot_stat_key = ftok(FTOK_PATH, FTOK_TOT_STATS);
    key_t shm_seats_key = ftok(FTOK_PATH, FTOK_SEATS);
    key_t shm_macros_key = ftok(FTOK_PATH, FTOK_MACROS);
    key_t sem_key = ftok(FTOK_PATH, FTOK_SEM);
    key_t ticket_key = ftok(FTOK_PATH, FTOK_TICKET);

    int shmid_macros, shmid_daily_stats, shmid_tot_stats, shmid_seats;

    if (shm_daily_stat_key == -1 || shm_tot_stat_key == -1 || shm_seats_key == -1 ||
        shm_macros_key == -1 || sem_key == -1 || ticket_key == -1) {
        perror("ftok failed");
        exit(EXIT_FAILURE);
    }

    shmid_macros = shmget(shm_macros_key, sizeof(shared_data), 0); 
    if(shmid_macros == -1){
        perror("shmget in worker for macros");
        raise(SIGTERM);
    }
    *shared_macros = shmat(shmid_macros, NULL, 0);
    if (*shared_macros == (void *)-1) {
        perror("shmat failed for macros in worker");
        raise(SIGTERM);
    }

    shmid_daily_stats = shmget(shm_daily_stat_key, (*shared_macros)->SIM_DURATION * sizeof(daily_stats), 0);
    if(shmid_daily_stats == -1){
        perror("shmget in worker for daily stats");
        raise(SIGTERM);
    }
    *shared_daily_stats = shmat(shmid_daily_stats, NULL, 0);
    if (*shared_daily_stats == (void *)-1) {
        perror("shmat failed for stats in worker");
        raise(SIGTERM);
    }

    shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), 0);
    if(shmid_tot_stats == -1){
        perror("shmget in worker for tot stats");
        raise(SIGTERM);
    }
    *shared_tot_stats = shmat(shmid_tot_stats, NULL, 0);
    if (*shared_tot_stats == (void *)-1) {
        perror("shmat failed for stats in worker");
        raise(SIGTERM);
    }

    shmid_seats = shmget(shm_seats_key, (*shared_macros)->NOF_WORKERSEATS * sizeof(worker_seat), 0);
    if(shmid_seats == -1){
        perror("shmget in worker for seats");
        raise(SIGTERM);
    }
    *shared_seats = shmat(shmid_seats, NULL, 0);
    if (*shared_seats == (void *)-1) {
        perror("shmat failed for seats in workerg");
        raise(SIGTERM);
    }

    *semid = semget(sem_key, (*shared_macros)->NOF_WORKERSEATS + worker_seats, 0);
    if(*semid == -1){
        perror("semid");
        raise(SIGTERM);
    }

    *msgid_ticket = msgget(ticket_key, 0);
    if(*msgid_ticket == -1){
        perror("msgget");
        raise(SIGTERM);
    }

    *msgid = malloc((*shared_macros)->NOF_WORKERSEATS * sizeof(int));
    for (int i = 0; i < (*shared_macros)->NOF_WORKERSEATS; i++){
        key_t msgworker_key = ftok(FTOK_PATH, FTOK_WORKER_BASE + i);
        (*msgid)[i] = msgget(msgworker_key, IPC_CREAT | 0666);
    }
}

void initialize_keys_modified(key_t *shm_data_key, key_t *sem_key, key_t *msg_key, key_t *shm_seats_key) {
    *shm_data_key = ftok(FTOK_PATH, FTOK_MACROS);
    *sem_key = ftok(FTOK_PATH, FTOK_SEM);
    *msg_key = ftok(FTOK_PATH, FTOK_TICKET);
    *shm_seats_key = ftok(FTOK_PATH, FTOK_SEATS);

    if (*shm_data_key == -1 || *sem_key == -1 || *msg_key == -1 || *shm_seats_key == -1) {
        perror("ftok failed");
        exit(EXIT_FAILURE);
    }
}

void reset_signals_to_default() {
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
}

void msgrcv_wait(int msgid, struct message *msg, size_t size, long type, int semid){
    msg->num = -1;
    while(get_semaphore_value(semid, end_day) == 0 && msg->num == -1){
        msgrcv(msgid, msg, size, type, IPC_NOWAIT);
        usleep(500);
    }
}

bool exist(int i , bool tasks_done[], int tasks[]){
    bool res = false;
    for(int j = 0; j < i && !res; j++){
        if(tasks[i] == tasks[j]){
            if(tasks_done[j]) res = true;   
        }
    }
    return res;
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

int read_parameter(const char *file_path, const char *parameter) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char line[30];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0;

        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (key && value && strcmp(key, parameter) == 0) {
            fclose(file);
            return atoi(value);
        }
    }

    fclose(file);
    fprintf(stderr, "Parameter '%s' not found in %s\n", parameter, file_path);
    exit(EXIT_FAILURE);
}