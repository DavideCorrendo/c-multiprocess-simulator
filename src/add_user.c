#include "main.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>

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

    // Ottieni le chiavi IPC esistenti
    key_t shm_macros_key = ftok("/tmp", 'D');
    key_t sem_key = ftok("/tmp", 'E');
    
    // Attacca alla memoria condivisa
    int shmid_macros = shmget(shm_macros_key, sizeof(int) * NUM_MACROS, 0666);
    int *shared_macros = (int*)shmat(shmid_macros, NULL, 0);
    
    // Ottieni il semaforo
    int semid = semget(sem_key, 0, 0666);
    
    // Semaforo per modificare shared_macros[7] (NOF_USERS)
    struct sembuf op = {0, -1, 0}; // Lock
    semop(semid, &op, 1);
    
    shared_macros[7] += new_users; 
    
    op.sem_op = 1; // Unlock
    semop(semid, &op, 1);
    
    // Crea nuovi processi utente
    for (int i = 0; i < new_users; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            execl("./bin/user", "bin/user", NULL);
            perror("execl failed");
            exit(EXIT_FAILURE);
        }
        else if (pid > 0) {
            // Aggiorna contatore processi in modo sicuro
            wait_semaphore(semid, shared_macros[1] + 1);
            shared_macros[5]++; // Processi attesi
            signal_semaphore(semid, shared_macros[1] + 1);
        }
    }
    
    shmdt(shared_macros);
    return 0;
}