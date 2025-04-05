#include "main.h"

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
void load_config(const char *filename, int NOF_WORKERS, int NOF_WORKERSEATS, int NOF_USERS, int NOF_PAUSE,
int N_REQUEST, int P_SERVE_MIN, int P_SERVE_MAX);
FILE *fp;


static struct message msg;
static int shmid_daily_stats = -1;
static int shmid_tot_stats = -1;
static int shmid_seats = -1;
static int shmid_macros = -1;
static int msgid = -1;
static int semid = -1;
static daily_stats **shared_daily_stats = NULL;
static tot_stats *shared_tot_stats = NULL;
static worker_seat *shared_seats = NULL;
static shared_data *shared_macros = NULL;
static pid_t *child_pids;
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
    reset_signals_to_default();
    cleanup();
    exit(EXIT_FAILURE);
}

void handle_termination(int sig) {
    // Cleanup code here - implement based on your needs
    printf("Received termination signal %d. Cleaning up...\n", sig);
    
    cleanup();
    
    exit(EXIT_FAILURE);
}


int main(int argc, char **argv) {
    setup_signal_handlers();     

    const char *file_explode = "config_explode.conf";

    EXPLODE_THRESHOLD = leggi_parametro(file_explode, "EXPLODE_THRESHOLD");

    int NOF_WORKERS;
    int NOF_WORKERSEATS;
    int NOF_USERS;
    int NOF_PAUSE;
    int N_REQUEST;
    int P_SERVE_MIN;
    int P_SERVE_MAX;

    load_config("config_timeout.conf", &NOF_WORKERS, &NOF_WORKERSEATS, &NOF_USERS, &NOF_PAUSE, &N_REQUEST,
        &P_SERVE_MIN, &P_SERVE_MAX);

    puts("program started");

    initialization_shm(SIM_DURATION, NOF_WORKERSEATS);

    shared_macros->NOF_WORKERS = NOF_WORKERS;
    shared_macros->NOF_WORKERSEATS = NOF_WORKERSEATS;
    shared_macros->current_day = 0;
    shared_macros->NOF_PAUSE = NOF_PAUSE; 
    shared_macros->timer = 0;
    shared_macros->N_REQUESTS = N_REQUEST;
    shared_macros->NOF_USERS = NOF_USERS;
    shared_macros->P_SERVE_MIN = P_SERVE_MIN;
    shared_macros->P_SERVE_MAX = P_SERVE_MAX;


    for(int i = 0; i < NOF_WORKERSEATS; i++) {
        shared_seats[i].id = i;
        shared_seats[i].busy = false;
        shared_seats[i].task = 0;  
        shared_seats[i].worker_id = 0;
    }

    for(int i = 0; i < SIM_DURATION; i++) {
        memset(&shared_daily_stats[i], 0, sizeof(daily_stats));
        for(int j = 0; j < shared_macros->NOF_WORKERSEATS; j++) {
            shared_tot_stats->num_ratio_worker_user[j] = 0.0;
        }
    }

    memset(shared_tot_stats, 0, sizeof(tot_stats));


    for(int i = 0; i < shared_macros->NOF_WORKERSEATS + 5; i++){
        init_semaphore(semid, i, 1);
    }

    puts("initializzazion finished");

    pid_t pid = fork();
    if (pid == 0) {
        execv("./bin/ticket_erogator", (char*[]){ "bin/ticket_erogator", NULL });
        perror("execv ticket_erogator failed");
        raise(SIGTERM);
    }
    add_child_pid(pid);

    for(int i = 1; i <= NOF_WORKERS; i++) {
        pid = fork();
        if (pid == 0) {
            char worker_id_str[32];
            snprintf(worker_id_str, sizeof(worker_id_str), "%d", i); 
            execv("./bin/worker", (char*[]){ "bin/worker", worker_id_str, NULL });
            perror("execv worker failed");
            raise(SIGTERM);
        }
        add_child_pid(pid);
    }
    

    for(int i = 0; i < NOF_USERS; i++) {
        pid = fork();
        if (pid == 0) {
            execv("./bin/user", (char*[]){ "bin/user", NULL });
            perror("execv user failed");
            raise(SIGTERM);
        }
        add_child_pid(pid);
    }

    fp = fopen("stats.csv", "a");
    if (fp == NULL) {
        perror("Error opening stats.csv");
        raise(SIGTERM);
    }


    for(; shared_macros->current_day < SIM_DURATION; shared_macros->current_day++){
        tasks_assignment(); 
        puts("task_assinment fatto");
        msg.mtype = 1;
        signal_semaphore(semid, 0);
        simulate_day();
        if(((shared_macros->NOF_WORKERS+shared_macros->NOF_USERS) - shared_macros->processes_finished) >= EXPLODE_THRESHOLD){
            printf("Simulation terminated: Number of waiting users exceeded threshold\n");
            break;
        } 
        wait_semaphore(semid, 0);
        signal_semaphore(semid, 1);
        wait_processes();
        wait_semaphore(semid, 1);
        puts("wait_process finito");
        //print_stats(shared_macros[8]);
        if(shared_macros->current_day == SIM_DURATION-1){signal_semaphore(semid, 4);}

        puts("messaggi mandati");
        reset_ipc();   
        puts("reset fatto");     
    }

    puts("FINITO TUTTO");

    print_file_stats();

    reset_signals_to_default();
    puts("resettato");
    sleep(2);
    cleanup();

    printf("Simulation completed successfully\n");
    return EXIT_SUCCESS;
}

void initialization_shm(int SIM_DURATION, int NOF_WORKERSEATS){
    

    key_t shm_daily_stat_key;
    key_t shm_tot_stat_key;
    key_t shm_seats_key;
    key_t shm_macros_key;
    key_t sem_key;
    key_t msg_key;

    initialize_keys(&shm_daily_stat_key, &shm_tot_stat_key, &shm_seats_key, &shm_macros_key, &sem_key, &msg_key);


    // Create shared memory segments for the actual structures, not pointers
    shmid_daily_stats = shmget(shm_daily_stat_key, SIM_DURATION * sizeof(daily_stats), IPC_CREAT | 0666);
    if (shmid_daily_stats == -1) {
        perror("shmget stats failed");
        raise(SIGTERM);
    }

    shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), IPC_CREAT | 0666);
    if (shmid_tot_stats == -1) {
        perror("shmget stats failed");
        raise(SIGTERM);
    }

    shmid_seats = shmget(shm_seats_key, NOF_WORKERSEATS * sizeof(worker_seat), IPC_CREAT | 0666);
    if (shmid_seats == -1) {
        perror("shmget seats failed in director");
        raise(SIGTERM);
    }

    shmid_macros = shmget(shm_macros_key, sizeof(shared_data), IPC_CREAT | 0666);
    if (shmid_macros == -1) {
        perror("shmget macros failed");
        raise(SIGTERM);
    }

    // Attach shared memory
    shared_daily_stats = (daily_stats **)shmat(shmid_daily_stats, NULL, 0);
    if (shared_daily_stats == (void *)-1) {
        perror("shmat stats failed");
        raise(SIGTERM);
    }

    shared_tot_stats = (tot_stats*)shmat(shmid_tot_stats, NULL, 0);
    if (shared_tot_stats == (void *)-1) {
        perror("shmat stats failed");
        raise(SIGTERM);
    }

    shared_seats = (worker_seat *)shmat(shmid_seats, NULL, 0);
    if (shared_seats == (void *)-1) {
        perror("shmat seats failed");
        raise(SIGTERM);
    }

    shared_macros = (shared_data *) shmat(shmid_macros, NULL, 0);
    if (shared_macros == (void *)-1) {
        perror("shmat macros failed");
        raise(SIGTERM);
    }

    msgid = msgget(msg_key, IPC_CREAT | 0666);
    if (msgid == -1) {
        perror("msgget failed in director");
        raise(SIGTERM);
    }

    semid = semget(sem_key, 100, IPC_CREAT | 0666);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }
    
}

void simulate_day() {
    
    while(shared_macros->timer < 720){
        usleep(N_NANO_SECS / 1000);
        shared_macros->timer++;
    }

}

void print_stats(int day){

    printf("\n\n-----------------------DAY %d-----------------------\n\n", day);
    printf("total number of user served: %d\n", shared_tot_stats->num_user_served);
    printf("average number of users served per worker: %.2f\n", shared_daily_stats[day]->avg_num_users_daily);
    printf("total number of services done: %d\n", shared_tot_stats->num_task_done);
    printf("total number of services not done: %d\n", shared_tot_stats->num_task_not_done);
    printf("average number of services done: %.2f\n", shared_daily_stats[day]->avg_num_tasks_done_daily);
    printf("average number of services not done: %.2f\n", shared_daily_stats[day]->avg_num_tasks_not_done_daily);
    printf("total average waiting time: %.2f\n", shared_tot_stats->avg_time_wait);
    printf("daily average waiting time: %.2f\n", shared_daily_stats[day]->avg_time_users_wait_daily);
    printf("total average service time: %.2f\n", shared_tot_stats->avg_time_task);
    printf("daily average service time: %.2f\n\n", shared_daily_stats[day]->avg_time_tasks_done_daily);

    for(int i = 0; i < 6; i++){
        printf("service %d\n", i);
        printf("total number of user served per service: %d\n", shared_tot_stats->num_user_served_per_task[i]);
        printf("average number of users served per worker per service: %.2f\n", shared_daily_stats[day]->avg_num_users_daily_per_task[i]);
        printf("total number of services done per service: %d\n", shared_tot_stats->num_task_done_per_task[i]);
        printf("total number of services not done per service: %d\n", shared_tot_stats->num_task_not_done_per_task[i]);
        printf("average number of services done per service: %.2f\n", shared_daily_stats[day]->avg_num_tasks_done_daily_per_task[i]);
        printf("average number of services not done per service: %.2f\n", shared_daily_stats[day]->avg_num_tasks_not_done_daily_per_task[i]);
        printf("total average waiting time per service: %.2f\n", shared_tot_stats->avg_time_wait_per_task[i]);
        printf("daily average waiting time per service: %.2f\n", shared_daily_stats[day]->avg_time_users_wait_daily_per_task[i]);
        printf("total average service time per service: %.2f\n", shared_tot_stats->avg_time_task_per_task[i]);
        printf("daily average service time per service: %.2f\n\n", shared_daily_stats[day]->avg_time_tasks_done_daily_per_task[i]);
    }

    printf("number of users active sor the simulation: %d\n",  shared_tot_stats->num_worker_active);
    printf("number of users active daily: %d\n", shared_daily_stats[day]->num_workers_active_daily);
    printf("average number of pause daily: %.2f\n", shared_daily_stats[day]->avg_num_pause_daily);
    printf("number of pause during simulation: %d\n", shared_tot_stats->num_pause);
    
    for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++){
        printf("ratio between workers and workerseats for workerseat[%d]: %.2f\n", i, shared_tot_stats->num_ratio_worker_user[i]);
    }

}

void print_file_stats() {
    fprintf(fp, "daily stats\n");
    // Write CSV header
    fprintf(fp, "day");
    fprintf(fp, ",daily_avg_users_per_worker");
    fprintf(fp, ",daily_avg_tasks_done");
    fprintf(fp, ",daily_avg_tasks_not_done");
    fprintf(fp, ",daily_avg_waiting_time");
    fprintf(fp, ",daily_avg_service_time");
    
    fprintf(fp, ",daily_workers_active");
    fprintf(fp, ",daily_avg_pause");

    // Service columns for each of the 6 services
    for (int s = 0; s < 6; s++) {
        fprintf(fp, ",service%d_daily_avg_users_per_worker", s);
        fprintf(fp, ",service%d_daily_avg_tasks_done", s);
        fprintf(fp, ",service%d_daily_avg_tasks_not_done", s);
        fprintf(fp, ",service%d_daily_avg_waiting_time", s);
        fprintf(fp, ",service%d_daily_avg_service_time", s);
    }

    // Seat ratio columns
    for (int seat = 0; seat < shared_macros->NOF_WORKERSEATS; seat++) {
        fprintf(fp, ",ratio_worker_user_seat%d", seat);
    }

    fprintf(fp, "\n");

    // Write data for each day
    for (int day = 0; day < SIM_DURATION; day++) {
        fprintf(fp, "%d", day);

        // Main stats
        fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_users_daily);
        fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_tasks_done_daily);
        fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_tasks_not_done_daily);
        fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_time_users_wait_daily);
        fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_time_tasks_done_daily);
        fprintf(fp, ",%d", shared_daily_stats[day]->num_workers_active_daily);
        fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_pause_daily);

        // Service stats
        for (int s = 0; s < 6; s++) {
            //fprintf(fp, ",%d", shared_tot_stats->num_user_served_per_task[s]);
            fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_users_daily_per_task[s]);
            //fprintf(fp, ",%d", shared_tot_stats->num_t->sk_done_per_task[s]);
            //fprintf(fp, ",%d", shared_tot_stats->num_t->sk_not_done_per_task[s]);
            fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_tasks_done_daily_per_task[s]);
            fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_num_tasks_not_done_daily_per_task[s]);
            //fprintf(fp, ",%.2f", shared_tot_stats->avg->time_wait_per_task[s]);
            fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_time_users_wait_daily_per_task[s]);
            //fprintf(fp, ",%.2f", shared_tot_stats->avg->time_task_per_task[s]);
            fprintf(fp, ",%.2f", shared_daily_stats[day]->avg_time_tasks_done_daily_per_task[s]);
        }

        // Seat ratios
        for (int seat = 0; seat < shared_macros->NOF_WORKERSEATS; seat++) {
            fprintf(fp, ",%.2f", shared_tot_stats->num_ratio_worker_user[seat]);
        }

        fprintf(fp, "\n");
    }

    fprintf(fp, "total stats\n");

    fprintf(fp, "total_users_served");
    fprintf(fp, ",total_tasks_done");
    fprintf(fp, ",total_tasks_not_done");
    fprintf(fp, ",total_avg_waiting_time");
    fprintf(fp, ",total_avg_service_time");
    fprintf(fp, ",total_worker_active");
    fprintf(fp, ",total_pause");

    for (int s = 0; s < 6; s++) {
        fprintf(fp, ",service%d_total_users_served", s);
        fprintf(fp, ",service%d_total_tasks_done", s);
        fprintf(fp, ",service%d_total_tasks_not_done", s);
        fprintf(fp, ",service%d_total_avg_waiting_time", s);
        fprintf(fp, ",service%d_total_avg_service_time", s);
    }

    fprintf(fp, "\n");

    fprintf(fp, "%d", shared_tot_stats->num_user_served);
    fprintf(fp, ",%d", shared_tot_stats->num_task_done);
    fprintf(fp, ",%d", shared_tot_stats->num_task_not_done);
    fprintf(fp, ",%.2f", shared_tot_stats->avg_time_wait);
    fprintf(fp, ",%.2f", shared_tot_stats->avg_time_task);
    fprintf(fp, ",%d", shared_tot_stats->num_worker_active);
    fprintf(fp, ",%d", shared_tot_stats->num_pause);

    for (int s = 0; s < 6; s++) {
        fprintf(fp, ",%d", shared_tot_stats->num_user_served_per_task[s]);
        fprintf(fp, ",%d", shared_tot_stats->num_task_done_per_task[s]);
        fprintf(fp, ",%d", shared_tot_stats->num_task_not_done_per_task[s]);
        fprintf(fp, ",%.2f", shared_tot_stats->avg_time_wait_per_task[s]);
        fprintf(fp, ",%.2f", shared_tot_stats->avg_time_task_per_task[s]);
    }

    fprintf(fp, "\n\n");

}

void tasks_assignment() {
    for(int i = 0; i < shared_macros->NOF_WORKERSEATS; i++) {
        wait_semaphore(semid, 5 + i);
        shared_seats[i].task = i % 6;
        signal_semaphore(semid, 5 + i);
    }
}

int reset_ipc() {

    shared_macros->processes_finished = 0;

    /*union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } sem_union;*/
    
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
        
        // Wait for a short time to allow processes to terminate
        sleep(1);
        
        // Check if any processes need to be forced to terminate
        for (int i = 0; i < num_children; i++) {
            if (child_pids[i] > 0) {
                if (kill(child_pids[i], 0) == 0) {
                    // Process still exists, force terminate
                    kill(child_pids[i], SIGKILL);
                }
            }
        }
        
        free(child_pids);
        child_pids = NULL;
    }

    shmdt(shared_macros); 
    shmdt(shared_daily_stats);
    shmdt(shared_tot_stats); 
    shmdt(shared_seats);
    
    shmctl(shmid_macros, IPC_RMID, NULL);
    printf("shm [id:%d] removed\n", shmid_macros);
    shmctl(shmid_daily_stats, IPC_RMID, NULL);
    printf("shm [id:%d] removed\n", shmid_daily_stats);
    shmctl(shmid_tot_stats, IPC_RMID, NULL);
    printf("shm [id:%d] removed\n", shmid_tot_stats);
    shmctl(shmid_seats, IPC_RMID, NULL);
    printf("shm [id:%d] removed\n", shmid_seats);

    msgctl(msgid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    if (fp != NULL) {
    fclose(fp);
    }
}

void wait_processes(){

    printf("NUM_WORKER = %d   NUM_USER = %d\n", shared_macros->NOF_WORKERS, shared_macros->NOF_USERS);

    // Get message queue statistics
    /*struct msqid_ds queue_info;
    if (msgctl(msgid, IPC_STAT, &queue_info) == -1) {
        perror("msgctl failed");
    } else {
        printf("Messages in queue: %lu\n", (unsigned long)queue_info.msg_qnum);
        printf("Max queue bytes: %lu\n", (unsigned long)queue_info.msg_qbytes);
        printf("Current queue size: %lu bytes\n", (unsigned long)queue_info.msg_cbytes);
    }*/

    while(1) {
        if(shared_macros->processes_finished == shared_macros->NOF_WORKERS + shared_macros->NOF_USERS + 1) break;
        printf("%d\n",shared_macros->processes_finished);
        sleep(1);
    }

}

void load_config(const char *filename, int* NOF_WORKERS, int* NOF_WORKERSEATS, int* NOF_USERS, int* NOF_PAUSE,
    int* N_REQUEST, int* P_SERVE_MIN, int* P_SERVE_MAX) {

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Errore apertura file di configurazione");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Ignora righe vuote e commenti
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Rimuovi spazi e newline
        line[strcspn(line, "\n")] = 0;
        
        // Dividi chiave e valore
        char *key = strtok(line, " =");
        char *value = strtok(NULL, " =");
        
        if (!key || !value) continue;  // Formato non valido

        // Assegnazione valori
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
            *P_SERVE_MIN = atof(value);  // float per probabilità
        }
        else if (strcmp(key, "P_SERV_MAX") == 0) {
            *P_SERVE_MAX = atof(value);
        }
        else if (strcmp(key, "NOF_PAUSE") == 0) {
            *NOF_PAUSE = atoi(value);
        }
        else if (strcmp(key, "NOF_REQUESTS") == 0) {
            *N_REQUEST = atoi(value);
        }
        else {
            fprintf(stderr, "Parametro sconosciuto: %s\n", key);
        }
    }
    
    fclose(file);
    
    // Verifica valori minimi (esempio)
    if (SIM_DURATION <= 0) {
        fprintf(stderr, "SIM_DURATION deve essere > 0\n");
        exit(EXIT_FAILURE);
    }
}
