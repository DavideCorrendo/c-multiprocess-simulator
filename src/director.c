#include "main.h"

#define NOF_WORKERSEATS 20
#define NOF_WORKERS 25
#define NOF_USERS 50

void main(){



    worker_seat **seats = Create_seatwork(NOF_WORKERSEATS);


    return 1;
}

worker_seat **Create_seatwork(int num){
    worker_seat **workerseats = malloc(num * sizeof(worker_seat*));

    for(int i = 0; i < num; i++){
        workerseats[i] = malloc(sizeof(worker_seat));
        workerseats[i]->id = i;
        workerseats[i]->busy = false;
        workerseats[i]->task = 0;
    }

    return workerseats;

}