#include "main.h"

void main(int argc, char **argv) {
    // Read configuration parameters
    const char *file_timeout = "config_timeout.conf";
    const char *file_explode = "config_explode.conf";

    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");
    int explode_threshold = leggi_parametro(file_explode, "EXPLODE_THRESHOLD");

    // Get command line arguments
    int NOF_WORKERSEATS = atoi(argv[1]);
    int NOF_WORKERS = atoi(argv[2]);
    int NOF_USERS = atoi(argv[3]);
    unsigned long long int N_NANO_SEC = atoi(argv[4]);
    int N_OF_PAUSE = atoi(argv[5]);


    int shmid_stats;
    int shmid_seats;

    stats *shared_stats;
    worker_seat *shared_seats;

    // Create and initialize shared memory
    initialization_shm(&shmid_stats, &shmid_seats, SIM_DURATION, NOF_WORKERSEATS,
                       &shared_stats, &shared_seats);

    // Initialize the shared memory contents
    for(int i = 0; i < NOF_WORKERSEATS; i++) {
        shared_seats[i].id = i;
        shared_seats[i].busy = false;
        shared_seats[i].task = 0;
    }

    for(int i = 0; i < SIM_DURATION; i++) {
        shared_stats[i].tot_num_users = 0;
        shared_stats[i].avg_num_users = 0;
        shared_stats[i].tot_num_tasks_done = 0;
        shared_stats[i].tot_num_tasks_not_done = 0;
        shared_stats[i].avg_num_tasks_done = 0;
        shared_stats[i].avg_num_tasks_not_done = 0;
        shared_stats[i].avg_time_users_wait_tot = 0;
        shared_stats[i].avg_time_users_wait_daily = 0;
        shared_stats[i].avg_tasks_done_tot = 0;
        shared_stats[i].avg_tasks_done_daily = 0;
        //statistiche precedenti suddivise
        shared_stats[i].num_workers_active_daily = 0;
        shared_stats[i].num_workers_active_tot = 0;
        shared_stats[i].avg_num_pause_daily = 0;
        shared_stats[i].num_pause_tot = 0;
        shared_stats[i].num_ratio_worker_user = 0;
    }


    // Create message queue and semaphores
    struct message msg;
    msg.mtype = 0;
    msg.mtext[0] = "\0";
    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    int semid = semget(SEM_KEY, 3 + NOF_WORKERSEATS, IPC_CREAT | 0666);

    // Create all processes
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        execv("./ticket_erogator", (char*[]){ "ticket_erogator", NULL });
        perror("execv ticket_erogator failed");
        exit(1);
    }

    for(int i = 0; i < NOF_WORKERS; i++) {
        pid = fork();
        if (pid == 0) {
            execv("./worker", (char*[]){ "worker", NULL });
            perror("execv worker failed");
            exit(1);
        }
        msg.MACRO = NOF_WORKERS;
        msgsnd(msgid, &msg, sizeof(msg.MACRO), 0);
        msg.MACRO = NOF_WORKERSEATS;
        msgsnd(msgid, &msg, sizeof(msg.MACRO), 0);
    }

    for(int i = 0; i < NOF_USERS; i++) {
        pid = fork();
        if (pid == 0) {
            execv("./user", (char*[]){ "user", NULL });
            perror("execv user failed");
            exit(1);
        }
    }

//------------------------------------------------------------------------

    char *inizio = "inizio";
    char *fine = "fine";

    for(int i = 0; i < SIM_DURATION; i++){
        tasks_assignment(shared_seats);  //<---------------------------------------- FINIRE BASTARDO
        msg.mtype = 0;
        str(msg.mtext, inizio);
        msgsnd(msgid, &msg, sizeof(struct message), 0);
        simulate_day(N_NANO_SEC);
        strcpy(msg.mtext, fine);
        msgsnd(msgid, &msg, sizeof(struct message), 0);
        print_stats(shared_stats[i]);     //<--------------------------------------   FINIRE BASTARDO
    }


    //scrivere EXPLODE


//------------------------------------------------------------------------------
    // Cleanup
    shmdt(shared_stats);
    shmdt(shared_seats);
    
    shmctl(shmid_stats, IPC_RMID, NULL);
    shmctl(shmid_seats, IPC_RMID, NULL);

    msgctl(msgid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    exit(0);
}


void initialization_shm(int *shmid_stats, int *shmid_seats, int SIM_DURATION, int NOF_WORKERSEATS,
                       stats **shared_stats, worker_seat **shared_seats){
    
    // Create shared memory segments for the actual structures, not pointers
    *shmid_stats = shmget(SHM_KEY_STATS, SIM_DURATION * sizeof(stats), IPC_CREAT | 0666);
    if (*shmid_stats == -1) {
        perror("shmget stats failed");
        exit(EXIT_FAILURE);
    }

    *shmid_seats = shmget(SHM_KEY_SEATS, NOF_WORKERSEATS + 3, IPC_CREAT | 0666);
    if (*shmid_seats == -1) {
        perror("shmget seats failed");
        exit(EXIT_FAILURE);
    }

    // Attach shared memory
    *shared_stats = (stats *)shmat(*shmid_stats, NULL, 0);
    if (*shared_stats == (void *)-1) {
        perror("shmat stats failed");
        exit(EXIT_FAILURE);
    }

    *shared_seats = (worker_seat *)shmat(*shmid_seats, NULL, 0);
    if (*shared_seats == (void *)-1) {
        perror("shmat seats failed");
        exit(EXIT_FAILURE);
    }

    
}

void simulate_day(unsigned long long nanos_per_minute) {
    struct timespec start_time, current_time;
    unsigned long long elapsed_nanos = 0;
    int simulated_minutes = 0;

    // Record the starting time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("Simulating a day with 1 simulated minute = %llu nanoseconds of real time...\n", nanos_per_minute);

    while (simulated_minutes < 480) { // 480 minutes for 8h of work
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        // Calculate elapsed time in nanoseconds
        elapsed_nanos = (current_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                        (current_time.tv_nsec - start_time.tv_nsec);

        // Check if a simulated minute has passed
        if (elapsed_nanos >= nanos_per_minute * (simulated_minutes + 1)) {
            simulated_minutes++;
            //printf("Simulated Time: %02d:%02d (HH:MM)\n", simulated_minutes / 60, simulated_minutes % 60);
        }

        // Sleep for a short while to reduce CPU usage
        usleep(100);
    }

    printf("Simulation complete: A full day has passed in simulated time.\n");
}