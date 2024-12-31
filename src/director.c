#include "main.h"

void handle_child_exit(int sig) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Process %d terminated with status: ", pid);
            if (WEXITSTATUS(status) == EXIT_SUCCESS) {
                printf("SUCCESS\n");
            } else {
                printf("FAILURE (code %d)\n", WEXITSTATUS(status));
            }
        } else if (WIFSIGNALED(status)) {
            printf("Process %d killed by signal %d\n", pid, WTERMSIG(status));
        }
    }
}

void validate_inputs(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <NOF_WORKERSEATS> <NOF_WORKERS> <NOF_USERS> <N_OF_PAUSE>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    for (int i = 1; i < argc; i++) {
        if (atoi(argv[i]) <= 0) {
            fprintf(stderr, "Error: All arguments must be positive integers.\n");
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char **argv) {
    validate_inputs(argc, argv);

    struct sigaction sa;
    sa.sa_handler = handle_child_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    const char *file_timeout = "config_timeout.conf";
    const char *file_explode = "config_explode.conf";

    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");
    int explode_threshold = leggi_parametro(file_explode, "EXPLODE_THRESHOLD");

    int NOF_WORKERSEATS = atoi(argv[1]);
    int NOF_WORKERS = atoi(argv[2]);
    int NOF_USERS = atoi(argv[3]);
    int N_OF_PAUSE = atoi(argv[4]);

    int shmid_stats;
    int shmid_seats;
    int shmid_macros;

    stats *shared_stats;
    worker_seat *shared_seats;
    int *shared_macros;

    initialization_shm(&shmid_stats, &shmid_seats, &shmid_macros, SIM_DURATION, NOF_WORKERSEATS,
                       &shared_stats, &shared_seats, &shared_macros);

    for(int i = 0; i < NOF_WORKERSEATS; i++) {
        shared_seats[i].id = i;
        shared_seats[i].busy = false;
        shared_seats[i].task = 0;  
        shared_seats[i].worker_id = 0;
    }

    for(int i = 0; i < SIM_DURATION; i++) {
        memset(&shared_stats[i], 0, sizeof(stats));
        for(int j = 0; j < shared_macros[1]; j++) {
            shared_stats[i].num_ratio_worker_user[j] = 0.0;
        }
    }

    shared_macros[0] = NOF_WORKERS;
    shared_macros[1] = NOF_WORKERSEATS;
    shared_macros[2] = 0;
    shared_macros[3] = SIM_DURATION;
    shared_macros[4] = N_OF_PAUSE; 

    struct message msg;
    memset(&msg, 0, sizeof(struct message));

    int msgid = msgget(MSG_KEY, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    int semid = semget(SEM_KEY, 2 + NOF_WORKERSEATS, IPC_CREAT | 0666);
    if (semid == -1) {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == 0) {
        execv("./ticket_erogator", (char*[]){ "ticket_erogator", NULL });
        perror("execv ticket_erogator failed");
        exit(EXIT_FAILURE);
    }

    msg.mtype = 2;
    for(int i = 0; i < NOF_WORKERS; i++) {
        pid = fork();
        if (pid == 0) {
            char worker_id_str[32];
            snprintf(worker_id_str, sizeof(worker_id_str), "%d", i + 1); 
            execv("./worker", (char*[]){ "worker", worker_id_str, NULL });
            perror("execv worker failed");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i < NOF_USERS; i++) {
        pid = fork();
        if (pid == 0) {
            execv("./user", (char*[]){ "user", NULL });
            perror("execv user failed");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 1; i <= SIM_DURATION; i++){
        initSem(semid, 5 + NOF_WORKERSEATS);
        shared_macros[6] = 0;
        tasks_assignment(shared_seats, shared_stats[i], shared_macros); 
        msg.mtype = 1;
        strcpy(msg.mtext, "start");
        msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0);
        simulate_day(&shared_macros[2]);
        strcpy(msg.mtext, "end");
        msgsnd(msgid, &msg, strlen(msg.mtext) + 1, 0);
        if(num_user_waiting(semid, shared_macros) >= explode_threshold){
            printf("Simulation terminated: Number of waiting users exceeded threshold\n");
            break;
        } 
        print_stats(shared_stats[i]);     
        reset_ipc(semid, shared_macros[1] + 1);            
    }

    strcpy(msg.mtext, "end");
    msg.mtype = 5;
    msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);

    shmdt(shared_macros); 
    shmdt(shared_stats);
    shmdt(shared_seats);
    
    shmctl(shmid_macros, IPC_RMID, NULL);
    shmctl(shmid_stats, IPC_RMID, NULL);
    shmctl(shmid_seats, IPC_RMID, NULL);

    msgctl(msgid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    printf("Simulation completed successfully\n");
    return EXIT_SUCCESS;
}

void initialization_shm(int *shmid_stats, int *shmid_seats, int *shmid_macros, int SIM_DURATION, int NOF_WORKERSEATS,
                       stats **shared_stats, worker_seat **shared_seats, int **shared_macros){
    
    // Create shared memory segments for the actual structures, not pointers
    *shmid_stats = shmget(SHM_KEY_STATS, SIM_DURATION * sizeof(stats), IPC_CREAT | 0666);
    if (*shmid_stats == -1) {
        perror("shmget stats failed");
        exit(EXIT_FAILURE);
    }

    *shmid_seats = shmget(SHM_KEY_SEATS, NOF_WORKERSEATS * sizeof(worker_seat), IPC_CREAT | 0666);
    if (*shmid_seats == -1) {
        perror("shmget seats failed");
        exit(EXIT_FAILURE);
    }

    *shmid_macros = shmget(SHM_KEY_MACROS, sizeof(int) * NUM_MACROS, IPC_CREAT | 0666);
    if (*shmid_macros == -1) {
        perror("shmget macros failed");
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

    *shared_macros = (int*) shmat(*shmid_macros, NULL, 0);
    if (*shared_macros == (void *)-1) {
        perror("shmat macros failed");
        exit(EXIT_FAILURE);
    }
    
}

void simulate_day(int *simulated_minutes) {
    struct timespec start_time, current_time;
    unsigned long long elapsed_nanos = 0;

    // Record the starting time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    printf("Simulating a day with 1 simulated minute = %d nanoseconds of real time...\n", N_NANO_SEC);

    while (*simulated_minutes < 480) { // 480 minutes for 8h of work
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        // Calculate elapsed time in nanoseconds
        elapsed_nanos = (current_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                        (current_time.tv_nsec - start_time.tv_nsec);

        // Check if a simulated minute has passed
        if (elapsed_nanos >= N_NANO_SEC * (*simulated_minutes + 1)) {
            (*simulated_minutes)++;
            //printf("Simulated Time: %02d:%02d (HH:MM)\n", *simulated_minutes / 60, *simulated_minutes % 60);
        }

        // Sleep for a short while to reduce CPU usage
        //usleep(100);
    }

    printf("Simulation complete: A full day has passed in simulated time.\n");
}

void print_stats(stats stat){
    printf("\n=== Daily Statistics ===\n");
    printf("Users: Total=%d, Avg=%.2f\n", stat.tot_num_users_tot, stat.avg_num_users_daily);
    printf("Tasks: Done=%d, Not Done=%d\n", stat.tot_num_tasks_done, stat.tot_num_tasks_not_done);
    printf("Wait Time: Daily=%.2f, Total=%.2f\n", stat.avg_time_users_wait_daily, stat.avg_time_users_wait_tot);
    printf("Active Workers: %d, W/U Ratio: %.2f\n", stat.num_workers_active_daily, stat.num_ratio_worker_user);
}

void tasks_assignment(worker_seat *shared_seats, stats curr_stats, int *shared_macros) {
    int task_failures[6] = {0};  // For 6 services
    float failure_rates[6] = {0.0};
    
    // Calculate failure rates for each service
    for(int service = 0; service < 6; service++) {
        int total_tasks = curr_stats.prev_stats_tot_tasks_done[service] + 
                         curr_stats.prev_stats_tot_tasks_not_done[service];
        
        if (total_tasks > 0) {
            failure_rates[service] = (float)curr_stats.prev_stats_tot_tasks_not_done[service] / total_tasks;
            task_failures[service] = (int)(failure_rates[service] * 100);
        }
    }
    
    // Calculate total failures
    int total_failures = 0;
    for(int i = 0; i < 6; i++) {
        total_failures += task_failures[i];
    }
    
    // Reset all seats
    for(int i = 0; i < shared_macros[1]; i++) {
        shared_seats[i].busy = false;
        shared_seats[i].task = 0;
        shared_seats[i].worker_id = 0;
    }
    
    // Distribute seats based on failure rates
    int current_seat = 0;
    if(total_failures > 0) {
        for(int service = 0; service < 6; service++) {
            int seats = (task_failures[service] * shared_macros[1]) / total_failures;
            seats = (seats > 0 && failure_rates[service] > 0) ? seats : 1;
            
            for(int j = 0; j < seats && current_seat < shared_macros[1]; j++) {
                shared_seats[current_seat].task = service;
                current_seat++;
            }
        }
    }
    
    // Distribute remaining seats evenly
    while(current_seat < shared_macros[1]) {
        for(int service = 0; service < 6 && current_seat < shared_macros[1]; service++) {
            shared_seats[current_seat].task = service;
            current_seat++;
        }
    }
}

int reset_ipc(int semid, int num_sem) {
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } sem_union;
    
    // Reset all semaphores to 0
    unsigned short values[num_sem];
    for (int i = 0; i < num_sem; i++) {
        values[i] = 0;
    }
    
    sem_union.array = values;
    if (semctl(semid, 0, SETALL, sem_union) == -1) {
        return -1;
    }
    
    return 0;
}