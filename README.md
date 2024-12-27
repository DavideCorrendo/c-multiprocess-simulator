Correndo Davide   1104824 davide.correndo@edu.unito.it
Collura  Federico 1102786 federico.collura@edu.unito.it


messages queue:
1 = start/end day
2 = worker_id
3 = worker <---> user
4 = task_time


shared_macros:
0 = number of worker
1 = number of worker_seats
2 = timer
3 = SIM_DURATION
4 = NOF_PAUSE

semaphores: the first shared_macros[1] semaphores are for the seats, then 1 for shared_stats