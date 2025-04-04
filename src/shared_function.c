#include "main.h"

union semun arg;


// Reads a parameter from the specified file
int leggi_parametro(const char *file_path, const char *parametro) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Errore nell'apertura del file");
        exit(EXIT_FAILURE);
    }

    char line[30];
    while (fgets(line, sizeof(line), file)) {
        // Remove newline character if present
        line[strcspn(line, "\n")] = 0;

        // Look for the specified key
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (key && value && strcmp(key, parametro) == 0) {
            fclose(file);
            return atoi(value);
        }
    }

    fclose(file);
    fprintf(stderr, "Parameter '%s' not found in %s\n", parametro, file_path);
    exit(EXIT_FAILURE);
}

int semop_retry(int semid, struct sembuf *sops, size_t nsops) {
    int ret;
    do {
        ret = semop(semid, sops, nsops);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

// Performs a semaphore operation
int sem_operation(int semid, int sem_num, int op_value) {
    struct sembuf sem_op;
    
    // Configure the operation
    sem_op.sem_num = sem_num;  // Specify which semaphore in the set
    sem_op.sem_op = op_value;  // Operation value (-1 for wait, +1 for signal)
    sem_op.sem_flg = 0;        // No special flags

    // Perform the operation
    if (semop_retry(semid, &sem_op, 1) == -1) {
        perror("semop failed");
        raise(SIGTERM);
    }

    return 0;
}

// Waits on a semaphore (decrements)
int wait_semaphore(int semid, int sem_num) {
    return sem_operation(semid, sem_num, -1);
}

// Signals a semaphore (increments)
int signal_semaphore(int semid, int sem_num) {
    return sem_operation(semid, sem_num, 1);
}

// Initializes a specific semaphore in the set
int init_semaphore(int semid, int sem_num, int value) {
    arg.val = value;
    if (semctl(semid, sem_num, SETVAL, arg) == -1) {
        perror("semctl SETVAL failed");
        return -1;
    }
    return 0;
}

// Gets the value of a specific semaphore
int get_semaphore_value(int semid, int sem_num) {
    int val = semctl(semid, sem_num, GETVAL);
    if (val == -1) {
        perror("semctl GETVAL failed");
    }
    return val;
}

// Initializes all semaphores in a set
int initSem(int semid, int num_sems) {
    for (int i = 0; i < num_sems; i++) {
        if (init_semaphore(semid, i, 1) == -1) {
            return -1;
        }
    }
    return 0;
}

// Initializes shared memory keys
void initialize_keys(key_t *shm_daily_stat_key, key_t *shm_tot_stat_key, key_t *shm_seats_key, key_t *shm_data_key, key_t *sem_key, key_t *msg_key) {   //<------------da controllare------------------>
    *shm_daily_stat_key = ftok("/tmp", 'A');
    *shm_tot_stat_key = ftok("/tmp", 'B');
    *shm_seats_key = ftok("/tmp", 'C');
    *shm_data_key = ftok("/tmp", 'D');
    *sem_key = ftok("/tmp", 'E');
    *msg_key = ftok("/tmp", 'F');

    if (*shm_daily_stat_key == -1 || *shm_tot_stat_key == -1 || *shm_seats_key == -1 ||
        *shm_data_key == -1 || *sem_key == -1 || *msg_key == -1) {
        perror("ftok failed");
        exit(EXIT_FAILURE);
    }
}

// Counts waiting users in the simulation
int num_user_waiting(int semid, shared_data *shared_data) {
    int count = 0;
    for (int i = 0; i < shared_data->NOF_WORKERSEATS; i++) {
        int val = get_semaphore_value(semid, i);
        if (val > 0) {
            count += val;
        }
    }
    return count;
}

// Initializes a modified set of keys (example placeholder)
void initialize_keys_modified(key_t *shm_data_key, key_t *sem_key, key_t *msg_key, key_t *shm_seats_key) {
    *shm_data_key = ftok("/tmp", 'D');
    *sem_key = ftok("/tmp", 'E');
    *msg_key = ftok("/tmp", 'F');
    *shm_seats_key = ftok("/tmp", 'C');

    if (*shm_data_key == -1 || *sem_key == -1 || *msg_key == -1 || *shm_seats_key == -1) {
        perror("ftok failed");
        exit(EXIT_FAILURE);
    }
}

void change_msg(int *msgid){
    key_t msg_key = ftok("/tmp", 'F');

    if((*msgid = msgget(msg_key, 0)) == -1){
        perror("Failed to remake msg");
        raise(SIGTERM);
    }
}

void reset_signals_to_default() {
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGSEGV, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
}

