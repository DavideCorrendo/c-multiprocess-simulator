#include "main.h"

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
    union semun arg;
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
}

int worker_per_task(int* shared_macros, worker_seat *shared_seats, int task){
    int cont = 0;
    for(int i = 0; i < shared_macros[0]; i++){
        if(shared_seats[i].task == task){
            cont++;
        }
    }
    return cont;
}

void get_function(int *msgid, int *shmid_macros, int *shmid_stats, int *shmid_seats, int *semid){

    *msgid = msgget(MSG_KEY, 0);

    *shmid_macros = shmget(SHM_KEY_MACROS, sizeof(int) * NUM_MACROS, 0);
    if(shmid_macros == -1) {
        perror("shmget");
        exit(1);
    }

    *semid = semget(SEM_KEY, 100, 0);
    if(semid == -1) {
        perror("semget");
        exit(1);
    }

    *shmid_stats = shmget(SHM_KEY_STATS, sizeof(stats), 0);
    if(shmid_stats == -1) {
        perror("shmget");
        exit(1);
    }

    *shmid_seats = shmget(SHM_KEY_SEATS, 100 * sizeof(worker_seat), 0);
    if(shmid_seats == -1) {
        perror("shmget");
        exit(1);
    }

}


int num_user_waiting(int semid, int *shared_macros) {
    int total_waiting = 0;
    
    // Check only worker seat semaphores
    for (int i = 0; i < shared_macros[1]; i++) {
        // Get number of processes waiting for zero on this semaphore
        int waiting = semctl(semid, i, GETZCNT, arg);
        if (waiting != -1) {
            total_waiting += waiting;
        }
    }

    return total_waiting;
}