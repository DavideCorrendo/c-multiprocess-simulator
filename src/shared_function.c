#include "main.h"

union semun arg;

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
        printf("%s\n", key);
        char *value = strtok(NULL, "=");
        if (value != NULL) {
            char *endptr;
            long val = strtol(value, &endptr, 10);
            if (*endptr != '\0') {
                fprintf(stderr, "Invalid integer value for parameter '%s'\n", parametro);
                exit(EXIT_FAILURE);
            }
        return (int)val;
        }
    }

    fclose(file);
    fprintf(stderr, "Parametro '%s' non trovato in '%s'\n", parametro, file_path);
    exit(EXIT_FAILURE);
}

// Function to perform semaphore operation on a specific semaphore in the set
int sem_operation(int semid, int sem_num, int op_value) {
    struct sembuf sem_op;
    
    // Configure the operation
    sem_op.sem_num = sem_num;  // Specify which semaphore in the set
    sem_op.sem_op = op_value;  // Operation value (-1 for P, +1 for V)
    sem_op.sem_flg = 0;        // No special flags
    
    // Perform the operation
    return semop(semid, &sem_op, 1);
}

// Example usage functions
int wait_semaphore(int semid, int sem_num) {
    return sem_operation(semid, sem_num, -1);
}

int signal_semaphore(int semid, int sem_num) {
    return sem_operation(semid, sem_num, 1);
}

// Initialize a specific semaphore in the set
int init_semaphore(int semid, int sem_num, int value) {
    arg.val = value;
    return semctl(semid, sem_num, SETVAL, arg);
}

// Get the value of a specific semaphore
int get_semaphore_value(int semid, int sem_num) {
    return semctl(semid, sem_num, GETVAL, 0);
}

int initSem(int semid, int num_sems){
    for (int i = 0; i < num_sems; i++) {
        if (semctl(semid, i, SETVAL, 0) == -1) {
            perror("semctl error");
            exit(EXIT_FAILURE);
        }
    }
    return 1;
}


void initialize_keys(key_t *shm_daily_stat_key, key_t *shm_tot_stat_key, key_t *shm_seats_key, key_t *shm_macros_key, key_t *sem_key, key_t *msg_key){

    if((*shm_daily_stat_key = ftok("/tmp", 'A')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*shm_tot_stat_key = ftok("/tmp", 'B')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*shm_seats_key = ftok("/tmp", 'C')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*shm_macros_key = ftok("/tmp", 'D')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*sem_key = ftok("/tmp", 'E')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }
    if((*msg_key = ftok("/tmp", 'F')) == -1){
        perror("ftok: ");
        exit(EXIT_FAILURE);
    }

}

int num_user_waiting(int semid, int *shared_macros) {
    int total_waiting = 0;
    
    for (int i = 0; i < shared_macros[1]; i++) {
        int waiting = semctl(semid, i, GETZCNT, arg);
        if (waiting != -1) {
            total_waiting += waiting;
        }
    }

    return total_waiting;
}


