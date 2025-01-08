#include "main.h"

void initialization_shm(int *shmid_daily_stats, int *shmid_tot_stats, int *shmid_seats, int *shmid_macros, 
                       int SIM_DURATION, int NOF_WORKERSEATS, daily_stats **shared_daily_stats, 
                       tot_stats **shared_tot_stats, worker_seat **shared_seats, int **shared_macros, 
                       int *msgid, int *semid);
void simulate_day();
void print_stats(int day);
void tasks_assignment(int day);
int reset_ipc(int semid, int num_sem);
void cleanup();
void send_messages(int msgid, struct message *msg, size_t size, int type, int flag, char *s);
void wait_processes(int day);
FILE *fp;


static struct message msg;
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
static pid_t *child_pids;
static int num_children = 0;
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

void validate_inputs(int argc, char **argv) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <NOF_WORKERS> <NOF_WORKERSEAT> <NOF_USERS> <N_OF_PAUSE> <N_REQUEST>\n", argv[0]);
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

    const char *file_timeout = "config_timeout.conf";
    const char *file_explode = "config_explode.conf";

    int SIM_DURATION = leggi_parametro(file_timeout, "SIM_DURATION");
    int explode_threshold = leggi_parametro(file_explode, "EXPLODE_THRESHOLD");

    int NOF_WORKERS = atoi(argv[1]);
    int NOF_WORKERSEATS = atoi(argv[2]);
    int NOF_USERS = atoi(argv[3]);
    int N_OF_PAUSE = atoi(argv[4]);
    int N_REQUEST = atoi(argv[5]);

    puts("program started");

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
    shared_macros[5] = 0;
    shared_macros[6] = N_REQUEST;
    shared_macros[7] = NOF_USERS;


    for(int i = 0; i < shared_macros[1] + 3; i++){
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
        printf("user creato: %d\n", pid);
        add_child_pid(pid);
    }

    fp = fopen("stats.csv", "a");
    if (fp == NULL) {
        perror("Error opening stats.csv");
        raise(SIGTERM);
    }

    //<-----------------------USER IN CODA DA GESTIRE----------------------------->
    for(int i = 0; i < SIM_DURATION; i++){
        init_semaphore(semid, shared_macros[1] + 3, 0);
        tasks_assignment(i); 
        puts("task_assinment fatto");
        msg.mtype = 1;
        strcpy(msg.mtext, "start");
        send_messages(msgid, &msg, sizeof(struct message) - sizeof(long), 1, 0, "start"); 
        simulate_day();
        strcpy(msg.mtext, "end");
        send_messages(msgid, &msg, sizeof(struct message) - sizeof(long), 2, 0, "end");
        puts("messaggi mandati");
        if(num_user_waiting(semid, shared_macros) >= explode_threshold){
            printf("Simulation terminated: Number of waiting users exceeded threshold\n");
            break;
        } 
        wait_processes(i);
        puts("wait_process finito");
        print_stats(i);
        send_messages(msgid, &msg, sizeof(struct message) - sizeof(long), 5, 0, "no_end");
        puts("messaggi mandati");
        reset_ipc(semid, shared_macros[1] + 3);   
        puts("reset fatto");     
    }

    puts("FINITO TUTTO");
    send_messages(msgid, &msg, sizeof(struct message) - sizeof(long), 5, 0, "end_simulation");

    reset_signals_to_default();
    puts("resettato");
    sleep(2);
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
        raise(SIGTERM);
    }

    *shmid_tot_stats = shmget(shm_tot_stat_key, sizeof(tot_stats), IPC_CREAT | 0666);
    if (*shmid_tot_stats == -1) {
        perror("shmget stats failed");
        raise(SIGTERM);
    }

    *shmid_seats = shmget(shm_seats_key, NOF_WORKERSEATS * sizeof(worker_seat), IPC_CREAT | 0666);
    if (*shmid_seats == -1) {
        perror("shmget seats failed in director");
        raise(SIGTERM);
    }

    *shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, IPC_CREAT | 0666);
    if (*shmid_macros == -1) {
        perror("shmget macros failed");
        raise(SIGTERM);
    }

    // Attach shared memory
    *shared_daily_stats = (daily_stats *)shmat(*shmid_daily_stats, NULL, 0);
    if (*shared_daily_stats == (void *)-1) {
        perror("shmat stats failed");
        raise(SIGTERM);
    }

    *shared_tot_stats = (tot_stats*)shmat(*shmid_tot_stats, NULL, 0);
    if (*shared_tot_stats == (void *)-1) {
        perror("shmat stats failed");
        raise(SIGTERM);
    }

    *shared_seats = (worker_seat *)shmat(*shmid_seats, NULL, 0);
    if (*shared_seats == (void *)-1) {
        perror("shmat seats failed");
        raise(SIGTERM);
    }

    *shared_macros = (int*) shmat(*shmid_macros, NULL, 0);
    if (*shared_macros == (void *)-1) {
        perror("shmat macros failed");
        raise(SIGTERM);
    }

    *msgid = msgget(msg_key, IPC_CREAT | 0666);
    if (*msgid == -1) {
        perror("msgget failed in director");
        raise(SIGTERM);
    }

    *semid = semget(sem_key, 100, IPC_CREAT | 0666);
    if(*semid == -1) {
        perror("semget");
        exit(1);
    }
    
}

void simulate_day() {
    
    while(shared_macros[2] < 480){
        usleep(N_NANO_SEC / 1000);
        shared_macros[2]++;
    }

    printf("Simulation complete: A full day has passed in simulated time.\n");
}

void print_stats(int day){
    (void) day;

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

}

void tasks_assignment(int day) {
    (void)day;
    for(int i = 0; i < shared_macros[1]; i++) {
        wait_semaphore(semid, i);
        shared_seats[i].task = i % 6;
        signal_semaphore(semid, i);
    }
}

int reset_ipc(int semid, int num_sem) {

    shared_macros[5] = 0;

    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short *array;
    } sem_union;

    // Reset all semaphores to 0
    unsigned short *values = malloc(num_sem * sizeof(unsigned short));

    for (int i = 0; i < num_sem; i++) {
        values[i] = 1;
    }
    
    sem_union.array = values;
    if (semctl(semid, 0, SETALL, sem_union) == -1) {
        return -1;
    }
    
    shared_macros[2] = 0;

    free(values);

    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("Failed to remove message queue");
        raise(SIGTERM);
    }

    key_t msg_key = ftok("/tmp", 'F');
    if(msg_key == -1){
        perror("errore nella ftok");
        raise(SIGTERM);
    }

    if((msgid = msgget(msg_key, IPC_CREAT | 0666)) == -1){
        perror("Failed to remake msg");
        raise(SIGTERM);
    }

    for(int i = 0; i < shared_macros[0] + shared_macros[7] + 2; i++){
        signal_semaphore(semid, shared_macros[1] + 3);
    }

    printf("\n\n\n\n\nSEMAFORO VALE %d\n", get_semaphore_value(semid, shared_macros[1] + 3));


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

void send_messages(int msgid, struct message *msg, size_t size, int type, int flag, char *s){
    strcpy(msg->mtext, s);

    if(strcmp(s, "end") == 0){
        msg->num = -1;
        msg->mtype = 4;
        msgsnd(msgid, msg, size, flag);
        

        for(int i = 0; i < shared_macros[1]; i++){
            msg->mtype = 6 + i;
            msgsnd(msgid, msg, size, flag);
        }
    }else{
        msg->num = 0;
    }

    msg->mtype = type;
    for(int i = 0; i < shared_macros[0] + shared_macros[7] + 1; i++){
        msgsnd(msgid, msg, size, flag);
    }
}

void wait_processes(int day){


    (void)day;

    printf("NUM_WORKER = %d   NUM_USER = %d\n", shared_macros[0], shared_macros[7]);

    while(1){
        if(shared_macros[5] == shared_macros[0] + shared_macros[7] + 1)break;
        printf("%d\n", shared_macros[5]);
        sleep(1);
    }

}
