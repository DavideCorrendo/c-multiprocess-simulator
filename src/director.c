#include "../include/main.h"

volatile sig_atomic_t keep_running = 1;

void initialization_shm(int SIM_DURATION, int NOF_WORKERSEATS);
void handle_child_exit(int sig);
void handle_termination(int sig);
void simulate_day();
void print_stats(int day);
void tasks_assignment();
int reset_ipc();
void cleanup();
void wait_processes();
void print_file_stats();
void load_config(const char *filename, int* NOF_WORKERS, int* NOF_WORKERSEATS, int* NOF_USERS, int* NOF_PAUSE,
int* N_REQUEST, int* P_SERVE_MIN, int* P_SERVE_MAX);
void initialize_semaphore(int semid);
int leggi_parametro(const char *file_path, const char *parametro);

static int shmid_daily_stats = -1;
static int shmid_tot_stats = -1;
static int shmid_seats = -1;
static int shmid_macros = -1;
static int ticket_msgid = -1;      
static int *worker_msgids = NULL; 
static int semid = -1;
static daily_stats *shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
static pid_t *child_pids = NULL;
static int num_children = 0;
static int SIM_DURATION;
static int EXPLODE_THRESHOLD;
FILE *fp;

void add_child_pid(pid_t pid) {
    num_children++;
    child_pids = realloc(child_pids, num_children * sizeof(pid_t));
    if (child_pids == NULL) {
        perror("Failed to allocate memory for child PIDs");
        exit(EXIT_FAILURE);
    }
    child_pids[num_children - 1] = pid;
}

void setup_signal_handlers() {
    struct sigaction sa;
    
    sa.sa_handler = handle_child_exit;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction SIGCHLD failed");
        exit(EXIT_FAILURE);
    }
    
    sa.sa_handler = handle_termination;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("sigaction SIGTERM failed");
        exit(EXIT_FAILURE);
    }
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction SIGINT failed");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction SIGSEGV failed");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("sigaction SIGHUP failed");
        exit(EXIT_FAILURE);
    }
}

void handle_child_exit(int sig) {
    (void)sig; 
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
    errno = saved_errno;  
}

void handle_termination(int sig) {
    (void)sig;
    keep_running = 0; 
}

int main() {
    puts("director process started");
    setup_signal_handlers();  

    EXPLODE_THRESHOLD = leggi_parametro("conf/config_explode.conf", "EXPLODE_THRESHOLD");

    int NOF_WORKERS, NOF_WORKERSEATS, NOF_USERS, NOF_PAUSE, N_REQUEST, P_SERVE_MIN, P_SERVE_MAX;

    puts("extracting and initializating variables...");
    load_config("conf/config_timeout.conf", &NOF_WORKERS, &NOF_WORKERSEATS, &NOF_USERS, &NOF_PAUSE, &N_REQUEST, &P_SERVE_MIN, &P_SERVE_MAX);
    initialization_shm(SIM_DURATION, NOF_WORKERSEATS);

    shared_macros->NOF_WORKERS = NOF_WORKERS;
    shared_macros->NOF_WORKERSEATS = NOF_WORKERSEATS;
    shared_macros->current_day = 0;
    shared_macros->NOF_PAUSE = NOF_PAUSE; 
    shared_macros->timer = 0;
    shared_macros->N_REQUESTS = N_REQUEST;
    shared_macros->NOF_USERS = NOF_USERS;
    shared_macros->P_SERV_MIN = P_SERVE_MIN;
    shared_macros->P_SERV_MAX = P_SERVE_MAX;
    shared_macros->USER_FINISHED = 0;
    shared_macros->SIM_DURATION = SIM_DURATION;

    for(int i = 0; i < NOF_WORKERSEATS; i++) {
        shared_seats[i].id = i;
        shared_seats[i].busy = false;
        shared_seats[i].task = 0;  
        shared_seats[i].worker_id = 0;
    }

    for(int i = 0; i < SIM_DURATION; i++) {
        memset(&shared_daily_stats[i], 0, sizeof(daily_stats));
    }
    memset(shared_tot_stats, 0, sizeof(tot_stats));

    initialize_semaphore(semid);

    puts("forking child processes...");
    pid_t pid = fork();
    if (pid == 0) {
        execv("bin/ticket_erogator", (char*[]){ "bin/ticket_erogator", NULL });
        perror("execv ticket_erogator failed");
        raise(SIGTERM);
    }
    add_child_pid(pid);

    for(int i = 1; i <= NOF_WORKERS; i++) {
        pid = fork();
        if (pid == 0) {
            char worker_id_str[32];
            snprintf(worker_id_str, sizeof(worker_id_str), "%d", i); 
            execv("bin/worker", (char*[]){ "bin/worker", worker_id_str, NULL });
            perror("execv worker failed");
            raise(SIGTERM);
        }
        add_child_pid(pid);
    }
    
    for(int i = 0; i < NOF_USERS; i++) {
        pid = fork();
        if (pid == 0) {
            execv("bin/user", (char*[]){ "bin/user", NULL });
            perror("execv user failed");
            raise(SIGTERM);
        }
        add_child_pid(pid);
    }

    puts("simulation started");
    for(; shared_macros->current_day < SIM_DURATION && keep_running; shared_macros->current_day++){
        tasks_assignment();
        signal_semaphore(semid, start_day);
        
        simulate_day();
        
        if((shared_macros->NOF_USERS - shared_macros->USER_FINISHED) >= EXPLODE_THRESHOLD){
            printf("Simulation terminated: Number of waiting users exceeded threshold\n");
            break;
        } 
        wait_semaphore(semid, start_day);
        if(shared_macros->current_day == SIM_DURATION-1) { signal_semaphore(semid, end_simulation); }
        
        signal_semaphore(semid, end_day);
        wait_processes();
        wait_semaphore(semid, end_day);
        
        print_stats(shared_macros->current_day);
        reset_ipc();  
    }

    fp = fopen("stats.csv", "a");
    if (fp != NULL) {
        print_file_stats();
        fclose(fp);
    }

    reset_signals_to_default();
    sleep(1);
    cleanup();
    printf("Simulation completed successfully\n");
    return EXIT_SUCCESS;
}

void initialization_shm(int SIM_DURATION, int NOF_WORKERSEATS){
    key_t shm_daily_stat_key = ftok(FTOK_PATH, FTOK_DAILY_STATS);
    key_t shm_tot_stat_key = ftok(FTOK_PATH, FTOK_TOT_STATS);
    key_t shm_seats_key = ftok(FTOK_PATH, FTOK_SEATS);
    key_t shm_macros_key = ftok(FTOK_PATH, FTOK_MACROS);
    key_t sem_key = ftok(FTOK_PATH, FTOK_SEM);
    key_t ticket_msg_key = ftok(FTOK_PATH, FTOK_TICKET); 

    shmid_daily_stats = shmget(shm_daily_stat_key, SIM_DURATION * sizeof(daily_stats), IPC_CREAT | 0666);
    shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), IPC_CREAT | 0666);
    shmid_seats = shmget(shm_seats_key, NOF_WORKERSEATS * sizeof(worker_seat), IPC_CREAT | 0666);
    shmid_macros = shmget(shm_macros_key, sizeof(shared_data), IPC_CREAT | 0666);

    shared_daily_stats = (daily_stats *)shmat(shmid_daily_stats, NULL, 0);
    shared_tot_stats = (tot_stats*)shmat(shmid_tot_stats, NULL, 0);
    shared_seats = (worker_seat *)shmat(shmid_seats, NULL, 0);
    shared_macros = (shared_data *) shmat(shmid_macros, NULL, 0);

    ticket_msgid = msgget(ticket_msg_key, IPC_CREAT | 0666);
    worker_msgids = malloc(NOF_WORKERSEATS * sizeof(int));
    for (int i = 0; i < NOF_WORKERSEATS; i++) {
        key_t worker_msg_key = ftok(FTOK_PATH, FTOK_WORKER_BASE + i); 
        worker_msgids[i] = msgget(worker_msg_key, IPC_CREAT | 0666);
    }

    semid = semget(sem_key, NOF_WORKERSEATS + worker_seats, IPC_CREAT | 0666);
}

void simulate_day() {
    while(shared_macros->timer < 720 && keep_running){
        usleep(N_NANO_SECS / 1000);
        shared_macros->timer++;
    }
}

void print_stats(int day){
    int num = shared_daily_stats[day].num_workers_active_daily;

    printf("\n\n-----------------------DAY %d-----------------------\n\n", day + 1);
    printf("total number of user served: %d\n", shared_tot_stats->num_user_served);
    printf("total number of services done: %d\n", shared_tot_stats->num_task_done);
    printf("total number of services not done: %d\n", shared_tot_stats->num_task_not_done);

    if(num != 0) {
        printf("average number of users served per worker: %.2f\n", (float)shared_daily_stats[day].user_served_daily / num);
        printf("average number of services done: %.2f\n", (float)shared_daily_stats[day].task_done / num);
        printf("average number of services not done: %.2f\n", (float)shared_daily_stats[day].task_not_done / num);
    }

    if(shared_daily_stats[day].task_done != 0) {
        printf("daily average waiting time: %.2f\n", (float)shared_daily_stats[day].daily_waiting_time / shared_daily_stats[day].task_done);
        printf("daily average service time: %.2f\n", (float)shared_daily_stats[day].time_task_daily / shared_daily_stats[day].task_done);
    }

    if(shared_tot_stats->num_task_done != 0) {
        printf("total average waiting time: %.2f\n", (float)shared_tot_stats->wait_time / shared_tot_stats->num_task_done);
        printf("total average service time: %.2f\n", (float)shared_tot_stats->task_time / shared_tot_stats->num_task_done);
    }

    printf("\n");

    for(int i = 0; i < NUM_TASKS; i++){
        int num2 = shared_tot_stats->num_worker_per_task[i];
        int num3 = shared_tot_stats->num_task_done_per_task[i];

        printf("service %d\n", i);
        printf("total number of services done: %d\n", shared_tot_stats->num_task_done_per_task[i]);
        printf("total number of user served: %d\n", shared_tot_stats->num_user_served_per_task[i]);
        printf("total number of services not done: %d\n", shared_tot_stats->num_task_not_done_per_task[i]);

        if(num2 != 0) {
            printf("average number of users served per worker: %.2f\n", (float)shared_daily_stats[day].user_served_per_task[i] / num2);
            printf("average number of services done: %.2f\n", (float)shared_daily_stats[day].task_done_per_task[i] / num2);
            printf("average number of services not done: %.2f\n", (float)shared_daily_stats[day].task_not_done_per_task[i] / num2);
        }

        if(num3 != 0) {
            printf("total average waiting time: %.2f\n", (float)shared_tot_stats->wait_time_per_task[i] / num3);
            printf("total average service time: %.2f\n", (float)shared_tot_stats->task_time_per_task[i] / num3);
        }

        if(shared_daily_stats[day].task_done_per_task[i] != 0) {
            printf("daily average waiting time: %.2f\n", (float)shared_daily_stats[day].time_wait_daily_per_task[i] / shared_daily_stats[day].task_done_per_task[i]);
            printf("daily average service time: %.2f\n", (float)shared_daily_stats[day].time_task_daily_per_task[i] / shared_daily_stats[day].task_done_per_task[i]);
        }

        printf("\n");
    }

    if(shared_daily_stats[day].num_workers_active_daily != 0) printf("average number of pause daily: %.2f\n", (float)shared_daily_stats[day].num_pause_daily / shared_daily_stats[day].num_workers_active_daily);
    printf("number of users active sor the simulation: %d\n",  shared_tot_stats->num_worker_active);
    printf("number of users active daily: %d\n", shared_daily_stats[day].num_workers_active_daily);
    printf("number of pause during simulation: %d\n", shared_tot_stats->num_pause);
    
    for(int i = 0; i < NUM_TASKS; i++){
        printf("ratio between workers and workerseats for workerseat[%d]: %.2f\n", i, shared_daily_stats[day].num_ratio_worker_user[i]);
    }
}

void print_file_stats() {
    time_t current_time;
    struct tm *time_info;
    char time_string[100];
    
    time(&current_time);
    time_info = localtime(&current_time);
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", time_info);
    
    fprintf(fp, "Statistics generated on: %s\n", time_string);
    fprintf(fp, "========================================\n");

    for(int day = 0; day < shared_macros->SIM_DURATION; day++) {
        int num = shared_daily_stats[day].num_workers_active_daily;
        fprintf(fp, "\n\n-----------------------DAY %d-----------------------\n\n", day + 1);
        
        if(num != 0) {
            fprintf(fp, "average number of users served per worker: %.2f\n", (float)shared_daily_stats[day].user_served_daily / num);
            fprintf(fp, "average number of services done: %.2f\n", (float)shared_daily_stats[day].task_done / num);
            fprintf(fp, "average number of services not done: %.2f\n", (float)shared_daily_stats[day].task_not_done / num);
        }
        if(shared_daily_stats[day].task_done != 0) {
            fprintf(fp, "average waiting time: %.2f\n", (float)shared_daily_stats[day].daily_waiting_time / shared_daily_stats[day].task_done);
            fprintf(fp, "average service time: %.2f\n\n", (float)shared_daily_stats[day].time_task_daily / shared_daily_stats[day].task_done);
        }
        
        for(int i = 0; i < NUM_TASKS; i++){
            fprintf(fp, "service %d daily stats\n", i + 1);
            if(shared_daily_stats[day].task_done_per_task[i] != 0) {
                fprintf(fp, "average waiting time: %.2f\n", (float)shared_daily_stats[day].time_wait_daily_per_task[i] / shared_daily_stats[day].task_done_per_task[i]);
                fprintf(fp, "average service time: %.2f\n", (float)shared_daily_stats[day].time_task_daily_per_task[i] / shared_daily_stats[day].task_done_per_task[i]);
            }
            fprintf(fp, "\n");
        }
        
        if(shared_daily_stats[day].num_workers_active_daily != 0) fprintf(fp, "average number of pause daily: %.2f\n", (float)shared_daily_stats[day].num_pause_daily / shared_daily_stats[day].num_workers_active_daily);
        fprintf(fp, "number of users active daily: %d\n", shared_daily_stats[day].num_workers_active_daily);
    
        for(int i = 0; i < NUM_TASKS; i++){
            fprintf(fp, "ratio between workers and workerseats for workerseat[%d]: %.2f\n", i, shared_daily_stats[day].num_ratio_worker_user[i]);
        }
    }
    
    fprintf(fp, "\n\n-----------------------TOTAL STATS-----------------------\n\n");
    fprintf(fp, "number of user served: %d\n", shared_tot_stats->num_user_served);
    fprintf(fp, "number of services done: %d\n", shared_tot_stats->num_task_done);
    fprintf(fp, "number of services not done: %d\n", shared_tot_stats->num_task_not_done);
    
    if(shared_tot_stats->num_task_done != 0) {
        fprintf(fp, "average waiting time: %.2f\n", (float)shared_tot_stats->wait_time / shared_tot_stats->num_task_done);
        fprintf(fp, "average service time: %.2f\n", (float)shared_tot_stats->task_time / shared_tot_stats->num_task_done);
    }
    
    fprintf(fp, "\n");
    for(int i = 0; i < NUM_TASKS; i++){
        int num2 = shared_tot_stats->num_worker_per_task[i];
        int num3 = shared_tot_stats->num_task_done_per_task[i];
        fprintf(fp, "service %d total stats\n", i);
        fprintf(fp, "number of services done: %d\n", shared_tot_stats->num_task_done_per_task[i]);
        fprintf(fp, "number of user served: %d\n", shared_tot_stats->num_user_served_per_task[i]);
        fprintf(fp, "number of services not done: %d\n", shared_tot_stats->num_task_not_done_per_task[i]);
        if(num2 != 0) {
            fprintf(fp, "number of users served per worker: %.2f\n", (float)shared_tot_stats->num_user_served_per_task[i] / num2);
            fprintf(fp, "number of services done: %.2f\n", (float)shared_tot_stats->num_task_done_per_task[i] / num2);
            fprintf(fp, "number of services not done: %.2f\n", (float)shared_tot_stats->num_task_not_done_per_task[i] / num2);
        }
        if(num3 != 0) {
            fprintf(fp, "average waiting time: %.2f\n", (float)shared_tot_stats->wait_time_per_task[i] / num3);
            fprintf(fp, "average service time: %.2f\n", (float)shared_tot_stats->task_time_per_task[i] / num3);
        }
        fprintf(fp, "\n");
    }
    
    fprintf(fp, "number of users active for the simulation: %d\n", shared_tot_stats->num_worker_active);
    fprintf(fp, "number of pause during simulation: %d\n", shared_tot_stats->num_pause);
}

void tasks_assignment() {
    if (shared_macros->current_day == 0) {
        for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++) {
            wait_semaphore(semid, worker_seats + i);
            shared_seats[i].task = i % NUM_TASKS;
            signal_semaphore(semid, worker_seats + i);
        }
        return;
    }
    
    int prev_day = shared_macros->current_day - 1;
    int task_demand[NUM_TASKS] = {0};
    
    for (int i = 0; i < NUM_TASKS; i++) {
        task_demand[i] = shared_daily_stats[prev_day].task_done_per_task[i] + 
                         shared_daily_stats[prev_day].task_not_done_per_task[i];
    }
    
    int seats_per_task[NUM_TASKS] = {0};
    int total_demand = 0;
    
    for (int i = 0; i < NUM_TASKS; i++) {
        total_demand += task_demand[i];
    }
    
    if (total_demand == 0) {
        for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++) {
            wait_semaphore(semid, worker_seats + i);
            shared_seats[i].task = i % NUM_TASKS;
            signal_semaphore(semid, worker_seats + i);
        }
        return;
    }
    
    int remaining_seats = shared_macros->NOF_WORKERSEATS - NUM_TASKS;
    
    for (int i = 0; i < NUM_TASKS; i++) {
        seats_per_task[i] = 1;
    }
    
    if (remaining_seats < 0) {
        for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++) {
            wait_semaphore(semid, worker_seats + i);
            shared_seats[i].task = i % NUM_TASKS;
            signal_semaphore(semid, worker_seats + i);
        }
        return;
    }
    
    if (remaining_seats > 0) {
        for (int i = 0; i < NUM_TASKS; i++) {
            int additional_seats = (task_demand[i] * remaining_seats) / total_demand;
            seats_per_task[i] += additional_seats;
            remaining_seats -= additional_seats;
        }
    }
    
    while (remaining_seats > 0) {
        float max_demand_ratio = -1;
        int max_task = 0;
        
        for (int i = 0; i < NUM_TASKS; i++) {
            float ratio = (float)task_demand[i] / seats_per_task[i];
                      
            if (ratio > max_demand_ratio) {
                max_demand_ratio = ratio;
                max_task = i;
            }
        }
        
        seats_per_task[max_task]++;
        remaining_seats--;
    }
    
    int seat_index = 0;
    for (int task_type = 0; task_type < NUM_TASKS; task_type++) {
        for (int j = 0; j < seats_per_task[task_type]; j++) {
            if (seat_index < shared_macros->NOF_WORKERSEATS) {
                wait_semaphore(semid, worker_seats + seat_index);
                shared_seats[seat_index].task = task_type;
                signal_semaphore(semid, worker_seats + seat_index);
                seat_index++;
            }
        }
    }
}

int reset_ipc() {
    shared_macros->processes_finished = 0;
    shared_macros->USER_FINISHED = 0;
    struct message msg;

    while (1) {
        ssize_t ret = msgrcv(ticket_msgid, &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);
        if (ret == -1) {
            if (errno == ENOMSG) break; 
            break;
        }
    }

    if (worker_msgids != NULL) {
        for (int i = 0; i < shared_macros->NOF_WORKERSEATS; i++) {
            while (1) {
                ssize_t ret = msgrcv(worker_msgids[i], &msg, sizeof(msg) - sizeof(long), 0, IPC_NOWAIT);
                if (ret == -1) {
                    if (errno == ENOMSG) break; 
                    break;
                }
            }
        }
    }
    
    shared_macros->timer = 0;
    return 0;
}

void cleanup(){
    if (child_pids != NULL) {
        for (int i = 0; i < num_children; i++) {
            if (child_pids[i] > 0) {
                kill(child_pids[i], SIGTERM);
            }
        }
        
        sleep(1);
        
        for (int i = 0; i < num_children; i++) {
            if (child_pids[i] > 0) {
                if (kill(child_pids[i], 0) == 0) {
                    kill(child_pids[i], SIGKILL);
                }
            }
        }
        
        free(child_pids);
        child_pids = NULL;
    }

    int nof_workerseats = (shared_macros != NULL) ? shared_macros->NOF_WORKERSEATS : 0;

    shmdt(shared_macros); 
    shmdt(shared_daily_stats);
    shmdt(shared_tot_stats); 
    shmdt(shared_seats);
    
    shmctl(shmid_macros, IPC_RMID, NULL);
    shmctl(shmid_daily_stats, IPC_RMID, NULL);
    shmctl(shmid_tot_stats, IPC_RMID, NULL);
    shmctl(shmid_seats, IPC_RMID, NULL);

    if (msgctl(ticket_msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl (ticket) failed");
    }

    if (worker_msgids != NULL && nof_workerseats > 0) {
        for (int i = 0; i < nof_workerseats; i++) {
            if (msgctl(worker_msgids[i], IPC_RMID, NULL) == -1) {
                perror("msgctl (worker seat) failed");
            }
        }
        free(worker_msgids);
        worker_msgids = NULL;
    }

    semctl(semid, 0, IPC_RMID);
}

void wait_processes(){
    while(keep_running) {
        if(shared_macros->processes_finished == shared_macros->NOF_WORKERS + shared_macros->NOF_USERS + 1) break;
        sleep(1);
    }
}

void load_config(const char *filename, int* NOF_WORKERS, int* NOF_WORKERSEATS, int* NOF_USERS, int* NOF_PAUSE, int* N_REQUEST, int* P_SERVE_MIN, int* P_SERVE_MAX) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Errore apertura file di configurazione");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        line[strcspn(line, "\n")] = 0;
        
        char *key = strtok(line, " =");
        char *value = strtok(NULL, " =");
        
        if (!key || !value) continue; 

        if (strcmp(key, "SIM_DURATION") == 0) {
            SIM_DURATION = atoi(value);
        } 
        else if (strcmp(key, "NOF_WORKERS") == 0) {
            *NOF_WORKERS = atoi(value);
        }
        else if (strcmp(key, "NOF_WORKERSEATS") == 0) {
            *NOF_WORKERSEATS = atoi(value);
        }
        else if (strcmp(key, "P_SERV_MIN") == 0) {
            *P_SERVE_MIN = atoi(value); 
        }
        else if (strcmp(key, "P_SERV_MAX") == 0) {
            *P_SERVE_MAX = atoi(value);
        }
        else if (strcmp(key, "NOF_PAUSE") == 0) {
            *NOF_PAUSE = atoi(value);
        }
        else if (strcmp(key, "NOF_REQUEST") == 0) {
            *N_REQUEST = atoi(value);
        }
        else if (strcmp(key, "NOF_USERS") == 0) {
            *NOF_USERS = atoi(value);
        }
    }
    
    fclose(file);
    
    if (SIM_DURATION <= 0 || *NOF_WORKERS <= 0 || *NOF_WORKERSEATS <= 0 || *NOF_USERS <= 0) {
        fprintf(stderr, "SIM DURATION, NOF_WORKER, NOF_WORKERSEATS and NOF_USERS must be > 0\n");
        exit(EXIT_FAILURE);
    }

    if (*NOF_PAUSE < 0 || *P_SERVE_MIN < 0 || *P_SERVE_MAX < 0) {
        fprintf(stderr, "NOF_PAUSE, P_SERVE_MIN and P_SERVE_MAX must be >= 0\n");
        exit(EXIT_FAILURE);
    }

    if (*P_SERVE_MIN > 100 || *P_SERVE_MAX > 100) {
        fprintf(stderr, "P_SERVE_MIN and P_SERVE_MAX must be <= 100\n");
        exit(EXIT_FAILURE);
    }
}

void initialize_semaphore(int semid){
    init_semaphore(semid,0,0);
    init_semaphore(semid,1,0);
    init_semaphore(semid,2,1);
    init_semaphore(semid,3,0);
    init_semaphore(semid,4,1);
    init_semaphore(semid,5,1);
    for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++){
        init_semaphore(semid, worker_seats + i, 1);
    }
}

