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
