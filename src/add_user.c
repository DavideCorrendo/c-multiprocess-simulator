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
    key_t shm_data_key = ftok("/tmp", 'D'); //<----------------------da sistemare-------------------->
    
    // Attacca alla memoria condivisa
    int shmid_data = shmget(shm_data_key, sizeof(struct shared_data), 0666);//<------------da controllare perchè sostituito a NUM_MACROS------>
    shared_data *shared_data = shmat(shmid_data, NULL, 0);
    
    shared_data->NOF_USERS += new_users; 
    
    
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

            shared_data->processes_finished++; // Processi attesi

        }
    }
    
    shmdt(shared_data);
    return 0;
}
