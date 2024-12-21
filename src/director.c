#include "main.h"

void main(int argc, char **argv){

    int NOF_WORKERSEATS, NOF_WORKERS, NOF_USERS;

    NOF_WORKERSEATS = atoi(argv[1]);
    NOF_WORKERS = atoi(argv[2]);
    NOF_USERS = atoi(argv[3]);

    worker_seat **seats = Create_seatwork(NOF_WORKERSEATS);


    return 1;
}

worker_seat **Create_seatwork(int num){
    worker_seat **workerseats = malloc(num * sizeof(worker_seat*));

    if(workerseats == NULL) return NULL;

    for(int i = 0; i < num; i++){
        workerseats[i] = malloc(sizeof(worker_seat));
        workerseats[i]->id = i;
        workerseats[i]->busy = false;
        workerseats[i]->task = 0;
    }

    return workerseats;

}