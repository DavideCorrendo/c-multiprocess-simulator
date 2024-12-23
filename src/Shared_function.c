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

int initSem(int semId, int value) {
    union semun arg;
    arg.val = value;
    return semctl(semId, 0, SETVAL, arg);
}

// Funzione per eseguire una P (wait) sul semaforo
int reserveSem(int semId) {
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = -1; // Decrementa il valore
    sops.sem_flg = 0;
    return semop(semId, &sops, 1);
}

// Funzione per eseguire una V (signal) sul semaforo
int releaseSem(int semId) {
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = 1; // Incrementa il valore
    sops.sem_flg = 0;
    return semop(semId, &sops, 1);
}
