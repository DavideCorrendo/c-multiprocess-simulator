Correndo Davide   1104824 davide.correndo@edu.unito.it
Collura  Federico 1102786 federico.collura@edu.unito.it


messages queue:
1 = start/end day
2 = worker_id
3 = ticket_erogator <---> director
4 = ticket_erogator <---> user
.
.
.
10 = worker <---> user
.
.
.
9 + shared_macros[0] = worker <---> user



shared_macros:
0 = number of worker
1 = number of worker_seats
2 = timer
3 = SIM_DURATION
4 = NOF_PAUSE
5 = UPGRADED
6 = NUM_USER_SERVED_DAILY
7 = NUM_USERS_NOT_SEREVD_DAILY
8 = TOT_WAITING_TIME (day 1)
.
.
* = TOT_WAITING_TIME (last day)
*+1 = TOT_TIME_TASK (day 1)
.
.
.
*+num_days = TOT_TIME_TASK (last day)

* = 7+num_days

semaphores: the first shared_macros[1] semaphores are for the seats, then 1 for shared_stats
--> shared_macros[1] for seats
--> 1 for stats
--> 1 for shared_macros[5]
--> 1 for shared_macros[6]  
--> 1 for shared_macros[7]
--> 1 for ticket