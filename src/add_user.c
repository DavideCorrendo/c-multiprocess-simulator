#include "../include/main.h"

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

    key_t shm_data_key = ftok("/tmp", 'D');
    key_t sem_key = ftok("/tmp", 'E');

    // Attach shared memory
    int shmid_data = shmget(shm_data_key, sizeof(struct shared_data), 0666);
    shared_data *shared_data = shmat(shmid_data, NULL, 0);

    int semid = semget(sem_key, shared_data->NOF_WORKERSEATS + worker_seats, 0);
    if(semid == -1){
        perror("semid");
        raise(SIGTERM);
    }
    
    wait_semaphore(semid, macros);
    shared_data->NOF_USERS += new_users;
    signal_semaphore(semid, macros); 
    
    // Create [new_users] new users
    for (int i = 0; i < new_users; i++) {
        pid_t pid = fork();
        if (pid == 0){
            execl("bin/user", "bin/user", NULL);
            perror("execl failed");
            shmdt(shared_data);
            exit(EXIT_FAILURE);
        }
    }
    
    shmdt(shared_data);
    return 0;
}
