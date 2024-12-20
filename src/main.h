#ifndef DIRECTOR
#define DIRECTOR

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/signal.h>

#define _POSIX_C_SOURCE 200809L

enum tasks{
        send_receive_parcels = 1,
        send_receive_letters_registered,
        withdrawals_deposits,
        bill_payments,
        purchase_financial_products,
        purchase_watches_bracelets,
};

typedef struct worker_seat{
    size_t id;
    enum tasks task;
    bool busy;
}worker_seat;

worker_seat **Create_seatwork(int );


#endif