# C Multi-Process Simulator: IPC Post Office

## Project Overview
This project is a multi-process architecture written in C that simulates the concurrent operation of a post office. It leverages System V POSIX Inter-Process Communication (IPC) primitives to manage parallel entities without data corruption or memory leaks.

The system demonstrates strict memory management, async-signal-safe error handling, and runtime process injection.

## Architecture & IPC Resources
The simulation is orchestrated by a `Director` process that initializes and schedules the lifecycle of `Users`, `Workers`, and a `Ticket Erogator`.

* **Shared Memory:**
  * `daily_stats`: Contains statistics for every day.
  * `tot_stats`: Contains statistics of all the simulation from day 0 to SIM_DURATION.
  * `worker_seats`: Contains all the data needed to manage the workers' seats (id, task, busy boolean, worker_id).
* **Semaphores:** Used for synchronous signaling between the director and child processes (`start_day`, `end_day`, `ticket_erogator`, `end_simulation`, `stats`, `macros`, `worker_seats`).
* **Message Queues:**
  * Ticket erogator <---> users (type 1 for ticket erogator, type 2 for users).
  * Workers <---> users (type 1 for worker, type 2 for users [id_worker]).

## Project Structure
```text
.
├── bin/                   # Compiled binaries and object files
├── conf/                  # Configuration files
│   ├── config_explode.conf
│   └── config_timeout.conf
├── include/
│   └── main.h             # Global macros, definitions, and IPC keys
├── src/
│   ├── add_user.c         # Dynamic runtime user injection
│   ├── director.c         # Main orchestrator and stats collector
│   ├── shared_function.c  # IPC wrappers and mathematical logic
│   ├── ticket_erogator.c  # Ticket distribution service
│   ├── user.c             # User logic and request simulator
│   └── worker.c           # Service desk operator
├── tests/
│   ├── test_integration.sh # E2E shell script for integration testing
│   └── test_logic.c        # Unit tests using assert.h
├── makefile               # Build and test automation
└── stats.csv              # Output log (Generated at runtime)
```

## Requirements

* Unix-like operating system.
* C compiler (GCC) with support for advanced strict options (`-Wvla`, `-Wextra`, `-Werror`, `-pedantic`).

## Compilation and Execution

1. Compile the project:

```bash
make all
```

2. Start the simulation:

```bash
./bin/director
```

3. Inject new users at runtime (Optional): Open a new terminal in the project root and run:

```bash
./bin/add_user <number_of_users>
```

All simulation parameters (e.g., maximum execution days, explode threshold, pause probability) are defined inside the `conf/` directory. Once the simulation ends naturally or hits the threshold, output is automatically saved in `stats.csv`.

## Testing
This project includes automated testing for both pure logic (Unit Tests) and full system behavior (Integration Tests).

* **Unit Tests:** Validates mathematical logic, array bounds, and config parsers without invoking IPC.

```bash
make test
```

* **Integration Tests:** A Bash script that builds the project, runs the simulation with a temporary configuration, injects dynamic users, and asserts clean exit codes and file generation.

```bash
./tests/test_integration.sh
```

## Cleanup
To remove all compiled objects and generated logs:

```bash
make clean
```
