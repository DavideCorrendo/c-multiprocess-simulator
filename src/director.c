#include "main.h"


void main(int argc, char **argv){

    const char *file_timeout = "config_timeout.conf";
    const char *file_explode = "config_explode.conf";

    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");
    int explode_threshold = leggi_parametro(file_explode, "EXPLODE_THRESHOLD");

    int NOF_WORKERSEATS, NOF_WORKERS, NOF_USERS, N_NANO_SEC, N_OF_PAUSE;

    NOF_WORKERSEATS = atoi(argv[1]);
    NOF_WORKERS = atoi(argv[2]);
    NOF_USERS = atoi(argv[3]);
    N_NANO_SEC = atoi(argv[4]);
    N_OF_PAUSE = atoi(argv[5]);

    stats **statistics = Create_statistics(SIM_DURATION);
    worker_seat **seats = Create_seatwork(NOF_WORKERSEATS);
    clock_t timer;

    int shmid_stats;
    int shmid_seats;
    int shmid_timer;

    stats **shared_stats;
    worker_seat **shared_seats;
    clock_t *shared_timer;

    initialization_shm(&shmid_stats, &shmid_seats, &shmid_timer, SIM_DURATION, NOF_WORKERSEATS, shared_stats, shared_seats, shared_timer);

    shared_stats = statistics;
    shared_seats = seats;
    shared_timer = &timer;

    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);

    int semid = semget(SEM_KEY, 3 + NOF_WORKERSEATS, IPC_CREAT | 0666);

//-------------------------------------------------------------------

    execv("ticket_erogator.c", "./ticket_erogator");

    for(int i = 0; i < NOF_WORKERS; i++){
        char *array_worker[2];
        array_worker[0] = "worker";
        array_worker[1] = NULL;
        execv("./worker", array_worker);
    }

    for(size_t i = 0; i < NOF_USERS; i++){
        char *array_user[2];
        array_user[0] = "user";
        array_user[1] = NULL;
        execv("./user", array_user);
    }

    return 1;
}

worker_seat **Create_seatwork(int num){
    worker_seat **workerseats = malloc(num * sizeof(worker_seat*));

    if(workerseats == NULL) return NULL;

    for(int i = 0; i < num; i++){
        workerseats[i] = malloc(sizeof(worker_seat));
        workerseats[i]->id = i;
        workerseats[i]->busy = false;
        workerseats[i]->task = 0;
    }

    return workerseats;

}

stats **Create_statistics(int num){
    stats **statistics = malloc(num * sizeof(stats*));

    for(int i = 0; i < num; i++){
        statistics[i] = malloc(sizeof(stats));

        statistics[i]->tot_num_users = 0;
        statistics[i]->avg_num_users = 0;
        statistics[i]->tot_num_tasks_done = 0;
        statistics[i]->tot_num_tasks_not_done = 0;
        statistics[i]->avg_num_tasks_done = 0;
        statistics[i]->avg_num_tasks_not_done = 0;
        statistics[i]->avg_time_users_wait_tot = 0;
        statistics[i]->avg_time_users_wait_daily = 0;
        statistics[i]->avg_tasks_done_tot = 0;
        statistics[i]->avg_tasks_done_daily = 0;
        statistics[i]->prec_stats = malloc(i * sizeof(stats*));
        for(int j = 0; j < i; j++){
            statistics[i]->prec_stats[j] = statistics[j];
        }
        statistics[i]->num_workers_active_daily = 0;
        statistics[i]->num_workers_active_tot = 0;
        statistics[i]->avg_num_pause_daily = 0;
        statistics[i]->num_pause_tot = 0;
        statistics[i]->num_ratio_worker_user = 0;
    }

    return statistics;
}

int leggi_parametro(const char *file_path, const char *parametro) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        exit(EXIT_FAILURE);
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        // Elimina il carattere di newline, se presente
        line[strcspn(line, "\n")] = 0;

        // Cerca la chiave specificata
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (key != NULL && value != NULL && strcmp(key, parametro) == 0) {
            fclose(file);
            return atoi(value); // Ritorna il valore come intero
        }
    }

    fclose(file);
    fprintf(stderr, "Parametro '%s' non trovato in '%s'\n", parametro, file_path);
    exit(EXIT_FAILURE);
}

void initialization_shm(int *shmid_stats, int *shmid_seats, int *shmid_timer, int SIM_DURATION, int NOF_WORKERSEATS, stats** shared_stats, worker_seat **shared_seats, clock_t *shared_timer){

    *shmid_stats = shmget(SHM_KEY_STATS, SIM_DURATION * sizeof(stats), IPC_CREAT | 0666);

    *shmid_seats = shmget(SHM_KEY_SEATS, NOF_WORKERSEATS * sizeof(worker_seat), IPC_CREAT | 0666);

    *shmid_timer = shmget(SHM_KEY_TIMER, sizeof(clock_t), IPC_CREAT | 0666);

    stats **shared_stats = (stats **)shmat(shmid_stats, NULL, 0);
    if (shared_stats == (void *)-1) {
        perror("Error attaching shared memory for stats");
        exit(EXIT_FAILURE);
    }

    worker_seat **shared_seats = (worker_seat **)shmat(shmid_seats, NULL, 0);
    if (shared_seats == (void *)-1) {
        perror("Error attaching shared memory for worker seats");
        exit(EXIT_FAILURE);
    }

    clock_t *shared_timer = (int *)shmat(shmid_timer, NULL, 0);
    if (shared_timer == (void *)-1) {
        perror("Error attaching shared memory for timer");
        exit(EXIT_FAILURE);
    }

}