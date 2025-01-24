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
1 = start day
2 = end day
3 = ticket_erogator <---> director
4 = ticket_erogator <---> user
5 = end simulation
6 = first workerseat
.
.
.
5 + shared_macros[1] = last worker

shared_macros:
0 = number of worker
1 = number of worker_seats
2 = timer
3 = SIM_DURATION
4 = NOF_PAUSE
5 = NUM_PROCESS_FINISHED
6 = N_REQUESTS
7 = NOF_USER

semaphores: the first shared_macros[1] semaphores are for the seats, then 1 for shared_stats
--> shared_macros[1] for seats
--> 1 for stats
--> 1 for macros
--> 1 for ticket
--> 1 for waiting processes