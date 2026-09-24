#include "../include/main.h"

volatile sig_atomic_t keep_running = 1;

void add_child_pid(pid_t pid);
void setup_signal_handlers();
void handle_child_exit(int sig);
void handle_termination(int sig);
void cleanup();

static int num_children = 0;
pid_t *child_pids = NULL;
static shared_data *shared_macros = NULL;

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

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <NUM_NEW_USERS>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int new_users = atoi(argv[1]);
    if (new_users <= 0) {
        fprintf(stderr, "Error: Number of users must be positive\n");
        exit(EXIT_FAILURE);
    }
    
    setup_signal_handlers();

    key_t shm_data_key = ftok(FTOK_PATH, FTOK_MACROS);
    key_t sem_key = ftok(FTOK_PATH, FTOK_SEM);

    int shmid_data = shmget(shm_data_key, sizeof(struct shared_data), 0666);
    if(shmid_data == -1) {
        perror("shmget in add_user");
        exit(EXIT_FAILURE);
    }
    
    shared_macros = shmat(shmid_data, NULL, 0);

    int semid = semget(sem_key, shared_macros->NOF_WORKERSEATS + worker_seats, 0);
    if(semid == -1){
        perror("semid");
        raise(SIGTERM);
    }
    
    wait_semaphore(semid, macros);
    shared_macros->NOF_USERS += new_users;
    signal_semaphore(semid, macros); 
    
    for (int i = 0; i < new_users && keep_running; i++) {
        pid_t pid = fork();
        if (pid == 0){
            execl("bin/user", "bin/user", NULL);
            perror("execl failed");
            shmdt(shared_macros);
            exit(EXIT_FAILURE);
        }
        add_child_pid(pid);
    }
    
    shmdt(shared_macros);
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
    if (shared_macros != NULL) {
        shmdt(shared_macros); 
    }
}