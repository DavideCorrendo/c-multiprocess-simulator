#include "main.h"

void initialization_shm(int *shmid_daily_stats, int *shmid_tot_stats, int *shmid_seats, int *shmid_macros, 
                       int SIM_DURATION, int NOF_WORKERSEATS, daily_stats **shared_daily_stats, 
                       tot_stats **shared_tot_stats, worker_seat **shared_seats, int **shared_macros, 
                       int *msgid, int *semid);
void simulate_day(int *simulated_minutes);
void print_stats(int day);
void tasks_assignment(int day);
int reset_ipc(int semid, int num_sem);

static int shmid_daily_stats = -1;
static int shmid_tot_stats = -1;
static int shmid_seats = -1;
static int shmid_macros = -1;
static int msgid = -1;
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static int *shared_macros = NULL;

void setup_signal_handlers() {
    struct sigaction sa;
    
    // SIGCHLD handler
    sa.sa_handler = handle_child_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction SIGCHLD failed");
        exit(EXIT_FAILURE);
    }
    
    // SIGTERM handler
    sa.sa_handler = handle_termination;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM failed");
        exit(EXIT_FAILURE);
    }
    
    // SIGINT handler
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT failed");
        exit(EXIT_FAILURE);
    }
}

void handle_child_exit(int sig) {
    int status;
    pid_t pid;
    int saved_errno = errno;  // Save errno
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Process %d terminated with signal %d status %d\n", 
                   pid, sig, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Process %d killed by signal %d%s\n", 
                   pid, WTERMSIG(status),
                   WCOREDUMP(status) ? " (core dumped)" : "");
        }
    }
    
    if (pid == -1 && errno != ECHILD) {
        perror("waitpid failed");
    }
    
    errno = saved_errno;  // Restore errno
    cleanup();
    exit(EXIT_FAILURE);
}

void handle_termination(int sig) {
    // Cleanup code here - implement based on your needs
    printf("Received termination signal %d. Cleaning up...\n", sig);
    
    cleanup();
    
    exit(EXIT_SUCCESS);
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
    setup_signal_handlers();     

    if(argc != 5){
        fprintf(stderr, "Usage: %s <NOF_WORKERSEATS> <NOF_WORKERS> <NOF_USERS> <N_OF_PAUSE>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *file_timeout = "config_timeout.conf";
    const char *file_explode = "config_explode.conf";

    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");
    int explode_threshold = leggi_parametro(file_explode, "EXPLODE_THRESHOLD");

    printf("%d\n", SIM_DURATION);
    printf("%d\n", explode_threshold);

    int NOF_WORKERSEATS = atoi(argv[1]);
    int NOF_WORKERS = atoi(argv[2]);
    int NOF_USERS = atoi(argv[3]);
    int N_OF_PAUSE = atoi(argv[4]);

    int shmid_daily_stats;
    int shmid_tot_stats;
    int shmid_seats;
    int shmid_macros;

    daily_stats *shared_daily_stats;
    tot_stats *shared_tot_stats;
    worker_seat *shared_seats;
    int *shared_macros;

    puts("program started");

    int msgid;
    int semid;

    initialization_shm(&shmid_daily_stats, &shmid_tot_stats, &shmid_seats, &shmid_macros, SIM_DURATION, NOF_WORKERSEATS,
                       &shared_daily_stats, &shared_tot_stats, &shared_seats, &shared_macros, &msgid, &semid);

    for(int i = 0; i < NOF_WORKERSEATS; i++) {
        shared_seats[i].id = i;
        shared_seats[i].busy = false;
        shared_seats[i].task = 0;  
        shared_seats[i].worker_id = 0;
    }

    for(int i = 0; i < SIM_DURATION; i++) {
        memset(&shared_daily_stats[i], 0, sizeof(daily_stats));
        for(int j = 0; j < shared_macros[1]; j++) {
            shared_daily_stats[i].num_ratio_worker_user[j] = 0.0;
        }
    }

    memset(shared_tot_stats, 0, sizeof(tot_stats));

    shared_macros[0] = NOF_WORKERS;
    shared_macros[1] = NOF_WORKERSEATS;
    shared_macros[2] = 0;
    shared_macros[3] = SIM_DURATION;
    shared_macros[4] = N_OF_PAUSE; 

    struct message msg;
    memset(&msg, 0, sizeof(struct message));

    puts("initializzazion finished");

    pid_t pid = fork();
    if (pid == 0) {
        execv("./bin/ticket_erogator", (char*[]){ "bin/ticket_erogator", NULL });
        perror("execv ticket_erogator failed");
        exit(EXIT_FAILURE);
    }

    puts("ticket erogator opened");

    msg.mtype = 2;
    for(int i = 0; i < NOF_WORKERS; i++) {
        pid = fork();
        if (pid == 0) {
            char worker_id_str[32];
            snprintf(worker_id_str, sizeof(worker_id_str), "%d", i + 1); 
            execv("./bin/worker", (char*[]){ "bin/worker", worker_id_str, NULL });
            perror("execv worker failed");
            exit(EXIT_FAILURE);
        }
    }

    
    puts("worker opened");
    

    for(int i = 0; i < NOF_USERS; i++) {
        pid = fork();
        if (pid == 0) {
            execv("./bin/user", (char*[]){ "bin/user", NULL });
            perror("execv user failed");
            exit(EXIT_FAILURE);
        }
    }

    puts("user_opened");

    for(int i = 1; i <= SIM_DURATION; i++){
        initSem(semid, 5 + NOF_WORKERSEATS);
        tasks_assignment(i); 
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
    
        print_stats(i);     
        reset_ipc(semid, shared_macros[1] + 1);            
    }

    strcpy(msg.mtext, "end");
    msg.mtype = 5;
    msgsnd(msgid, &msg, sizeof(struct message) - sizeof(long), 0);

    cleanup();

    printf("Simulation completed successfully\n");
    return EXIT_SUCCESS;
}

void initialization_shm(int *shmid_daily_stats, int *shmid_tot_stats, int *shmid_seats, int *shmid_macros, int SIM_DURATION, int NOF_WORKERSEATS,
                       daily_stats **shared_daily_stats, tot_stats **shared_tot_stats, worker_seat **shared_seats, int **shared_macros, int *msgid, int *semid){
    

    key_t shm_daily_stat_key;
    key_t shm_tot_stat_key;
    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys(&shm_daily_stat_key, &shm_tot_stat_key, &shm_seats_key, &shm_macros_key, &sem_key, &msg_key);

    // Create shared memory segments for the actual structures, not pointers
    *shmid_daily_stats = shmget(shm_daily_stat_key, SIM_DURATION * sizeof(daily_stats), IPC_CREAT | 0666);
    if (*shmid_daily_stats == -1) {
        perror("shmget stats failed");
        exit(EXIT_FAILURE);
    }

    *shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), IPC_CREAT | 0666);
    if (*shmid_tot_stats == -1) {
        perror("shmget stats failed");
        exit(EXIT_FAILURE);
    }

    *shmid_seats = shmget(shm_seats_key, NOF_WORKERSEATS * sizeof(worker_seat), IPC_CREAT | 0666);
    if (*shmid_seats == -1) {
        perror("shmget seats failed");
        exit(EXIT_FAILURE);
    }

    *shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, IPC_CREAT | 0666);
    if (*shmid_macros == -1) {
        perror("shmget macros failed");
        exit(EXIT_FAILURE);
    }

    // Attach shared memory
    *shared_daily_stats = (daily_stats *)shmat(*shmid_daily_stats, NULL, 0);
    if (*shared_daily_stats == (void *)-1) {
        perror("shmat stats failed");
        exit(EXIT_FAILURE);
    }

    *shared_tot_stats = (tot_stats*)shmat(*shmid_tot_stats, NULL, 0);
    if (*shared_tot_stats == (void *)-1) {
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

    *msgid = msgget(msg_key, IPC_CREAT | 0666);
    if (*msgid == -1) {
        perror("msgget failed");
        exit(EXIT_FAILURE);
    }

    *semid = semget(sem_key, 100, IPC_CREAT | 0666);
    if(*semid == -1) {
        perror("semget");
        exit(1);
    }
    
}

void simulate_day(int *simulated_minutes) {
    struct timespec start_time, current_time;
    unsigned long long elapsed_nanos = 0;

    // Record the starting time
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    while (*simulated_minutes < 480) { // 480 minutes for 8h of work
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        // Calculate elapsed time in nanoseconds
        elapsed_nanos = (current_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                        (current_time.tv_nsec - start_time.tv_nsec);

        // Check if a simulated minute has passed
        if (elapsed_nanos >= N_NANO_SEC * ((unsigned long long)(*simulated_minutes) + 1)) {
            (*simulated_minutes)++;
            //printf("Simulated Time: %02d:%02d (HH:MM)\n", *simulated_minutes / 60, *simulated_minutes % 60);
        }

        // Sleep for a short while to reduce CPU usage
        usleep(100);
    }

    printf("Simulation complete: A full day has passed in simulated time.\n");
}

void print_stats(int day){
    printf("\n\n-----------------------DAY %d-----------------------\n\n", day);
    printf("total number of user served: %d\n", shared_tot_stats->num_user_served);
    printf("average number of users served per worker: %.2f\n", shared_daily_stats[day].avg_num_users_daily);
    printf("total number of services done: %d\n", shared_tot_stats->num_task_done);
    printf("total number of services not done: %d\n", shared_tot_stats->num_task_not_done);
    printf("average number of services done: %.2f\n", shared_daily_stats[day].avg_num_tasks_done_daily);
    printf("average number of services not done: %.2f\n", shared_daily_stats[day].avg_num_tasks_not_done_daily);
    printf("total average waiting time: %.2f\n", shared_tot_stats->avg_time_wait);
    printf("daily average waiting time: %.2f\n", shared_daily_stats[day].avg_time_users_wait_daily);
    printf("total average service time: %.2f\n", shared_tot_stats->avg_time_task);
    printf("daily average service time: %.2f\n\n", shared_daily_stats[day].avg_time_tasks_done_daily);

    for(int i = 0; i < 6; i++){
        printf("service %d\n", i);
        printf("total number of user served per service: %d\n", shared_tot_stats->num_user_served_per_task[i]);
        printf("average number of users served per worker per service: %.2f\n", shared_daily_stats[day].avg_num_users_daily_per_task[i]);
        printf("total number of services done per service: %d\n", shared_tot_stats->num_task_done_per_task[i]);
        printf("total number of services not done per service: %d\n", shared_tot_stats->num_task_not_done_per_task[i]);
        printf("average number of services done per service: %.2f\n", shared_daily_stats[day].avg_num_tasks_done_daily_per_task[i]);
        printf("average number of services not done per service: %.2f\n", shared_daily_stats[day].avg_num_tasks_not_done_daily_per_task[i]);
        printf("total average waiting time per service: %.2f\n", shared_tot_stats->avg_time_wait_per_task[i]);
        printf("daily average waiting time per service: %.2f\n", shared_daily_stats[day].avg_time_users_wait_daily_per_task[i]);
        printf("total average service time per service: %.2f\n", shared_tot_stats->avg_time_task_per_task[i]);
        printf("daily average service time per service: %.2f\n\n", shared_daily_stats[day].avg_time_tasks_done_daily_per_task[i]);
    }

    printf("number of users active sor the simulation: %d\n", shared_daily_stats[day].num_workers_active_daily);;
    printf("number of users active daily: %d\n", shared_tot_stats->num_worker_active);
    printf("average number of pause daily: %.2f\n", shared_daily_stats[day].avg_num_pause_daily);
    printf("number of pause during simulation: %d\n", shared_tot_stats->num_pause);
    
    for(int i = 0; i < shared_macros[1]; i++){
        printf("ratio between workers and workerseats for workerseat[%d]: %.2f\n", i, shared_daily_stats[day].num_ratio_worker_user[i]);
    }

    FILE *fp;
    fp = fopen("stats.csv", "a");
    if (fp == NULL) {
        perror("Error opening stats.csv");
        return;
    }

    fprintf(fp, "Timestamp,Day,Total Users Served,Avg Users Per Worker,Total Services Done,Total Services Not Done,"
                "Avg Services Done,Avg Services Not Done,Total Avg Wait Time,Daily Avg Wait Time,"
                "Total Avg Service Time,Daily Avg Service Time");
        
    for (int i = 0; i < 6; i++) {
        fprintf(fp, ",Service %d Users,Service %d Avg Users Per Worker,Service %d Tasks Done,"
            "Service %d Tasks Not Done,Service %d Avg Tasks Done,Service %d Avg Tasks Not Done,"
            "Service %d Avg Wait Time,Service %d Daily Wait Time,"
            "Service %d Avg Service Time,Service %d Daily Service Time",
            i, i, i, i, i, i, i, i, i, i);
    }

    fprintf(fp, ",Active Users Simulation,Active Users Daily,Avg Daily Pauses,Total Pauses");
        
        for (int i = 0; i < shared_macros[1]; i++) {
            fprintf(fp, ",Worker-Seat Ratio %d", i);
        }
        fprintf(fp, "\n");

    time_t now;
    char timestamp[26];
    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Write data row
    fprintf(fp, "%s,%d,%d,%.2f,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
            timestamp,
            day,
            shared_tot_stats->num_user_served,
            shared_daily_stats[day].avg_num_users_daily,
            shared_tot_stats->num_task_done,
            shared_tot_stats->num_task_not_done,
            shared_daily_stats[day].avg_num_tasks_done_daily,
            shared_daily_stats[day].avg_num_tasks_not_done_daily,
            shared_tot_stats->avg_time_wait,
            shared_daily_stats[day].avg_time_users_wait_daily,
            shared_tot_stats->avg_time_task,
            shared_daily_stats[day].avg_time_tasks_done_daily);

    // Write per-service stats
    for (int i = 0; i < 6; i++) {
        fprintf(fp, ",%d,%.2f,%d,%d,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f",
                shared_tot_stats->num_user_served_per_task[i],
                shared_daily_stats[day].avg_num_users_daily_per_task[i],
                shared_tot_stats->num_task_done_per_task[i],
                shared_tot_stats->num_task_not_done_per_task[i],
                shared_daily_stats[day].avg_num_tasks_done_daily_per_task[i],
                shared_daily_stats[day].avg_num_tasks_not_done_daily_per_task[i],
                shared_tot_stats->avg_time_wait_per_task[i],
                shared_daily_stats[day].avg_time_users_wait_daily_per_task[i],
                shared_tot_stats->avg_time_task_per_task[i],
                shared_daily_stats[day].avg_time_tasks_done_daily_per_task[i]);
    }

    // Write worker stats
    fprintf(fp, ",%d,%d,%.2f,%d",
            shared_daily_stats[day].num_workers_active_daily,
            shared_tot_stats->num_worker_active,
            shared_daily_stats[day].avg_num_pause_daily,
            shared_tot_stats->num_pause);

    // Write worker-seat ratios
    for (int i = 0; i < shared_macros[1]; i++) {
        fprintf(fp, ",%.2f", shared_daily_stats[day].num_ratio_worker_user[i]);
    }
    fprintf(fp, "\n");

    fclose(fp);
}

void tasks_assignment(int day) {
    int num = 0;
    float proportions[6] = {0}; // Array to store task proportions

    if(day > 0){
        num = shared_daily_stats[day - 1].user_served_daily + shared_daily_stats[day - 1].user_not_served_daily;
        float num1 = shared_daily_stats[day - 1].user_served_per_task[0] + shared_daily_stats[day - 1].user_not_served_per_task[0];
        float num2 = shared_daily_stats[day - 1].user_served_per_task[1] + shared_daily_stats[day - 1].user_not_served_per_task[1];
        float num3 = shared_daily_stats[day - 1].user_served_per_task[2] + shared_daily_stats[day - 1].user_not_served_per_task[2];
        float num4 = shared_daily_stats[day - 1].user_served_per_task[3] + shared_daily_stats[day - 1].user_not_served_per_task[3];
        float num5 = shared_daily_stats[day - 1].user_served_per_task[4] + shared_daily_stats[day - 1].user_not_served_per_task[4];
        float num6 = shared_daily_stats[day - 1].user_served_per_task[5] + shared_daily_stats[day - 1].user_not_served_per_task[5];

        if(num != 0) {
            // Calculate proportions for each task type
            proportions[0] = num1 / num;
            proportions[1] = num2 / num;
            proportions[2] = num3 / num;
            proportions[3] = num4 / num;
            proportions[4] = num5 / num;
            proportions[5] = num6 / num;

            // Calculate cumulative proportions for assignment
            float cumulative = 0;
            for(int i = 0; i < shared_macros[1]; i++) {
                wait_semaphore(semid, i);
                
                // Determine task based on proportional distribution
                float random = (float)i / shared_macros[1]; // Distribute evenly across seats
                int assigned_task = 0;
                cumulative = 0;
                
                for(int j = 0; j < 6; j++) {
                    cumulative += proportions[j];
                    if(random <= cumulative) {
                        assigned_task = j;
                        break;
                    }
                }
                
                shared_seats[i].task = assigned_task;
                signal_semaphore(semid, i);
            }
        }
    } else {
        // Initial distribution for first day remains the same
        for(int i = 0; i < shared_macros[1]; i++) {
            wait_semaphore(semid, i);
            shared_seats[i].task = i % 6;
            signal_semaphore(semid, i);
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
    unsigned short *values = malloc(num_sem * sizeof(unsigned short));

    for (int i = 0; i < num_sem; i++) {
        values[i] = 0;
    }
    
    sem_union.array = values;
    if (semctl(semid, 0, SETALL, sem_union) == -1) {
        return -1;
    }
    
    free(values);
    return 0;
}

void cleanup(){
    shmdt(shared_macros); 
    shmdt(shared_daily_stats);
    shmdt(shared_tot_stats); 
    shmdt(shared_seats);
    
    shmctl(shmid_macros, IPC_RMID, NULL);
    shmctl(shmid_daily_stats, IPC_RMID, NULL);
    shmctl(shmid_tot_stats, IPC_RMID, NULL);
    shmctl(shmid_seats, IPC_RMID, NULL);

    msgctl(msgid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}