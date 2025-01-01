Correndo Davide   1104824 davide.correndo@edu.unito.it
Collura  Federico 1102786 federico.collura@edu.unito.it

ftok keys:
A = daily_stats 
B = tot_stats
C = seats
D = macros 
E = sem 
F = msg

messages queue:
1 = start/end day
2 = 
3 = ticket_erogator <---> director
4 = ticket_erogator <---> user
5 = end simulation
6 = first worker
.
.
.
5 + shared_macros[0] = last worker

shared_macros:
0 = number of worker
1 = number of worker_seats
2 = timer
3 = SIM_DURATION
4 = NOF_PAUSE
5 = TOT_SERVED
6 = TOT_TASK_DONE
7 = TOT_TASK_NOT_DONE
8 = TOT_TIME_WAIT
9 = TOT_TIME_TASK
10 = TOT_NUM_WORKER_ACTIVE
11 = NUM_PAUSE_TOT

semaphores: the first shared_macros[1] semaphores are for the seats, then 1 for shared_stats
--> shared_macros[1] for seats
--> 1 for stats
--> 1 for macros
--> 1 for ticket