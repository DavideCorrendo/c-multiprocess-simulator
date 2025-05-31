- Correndo Davide   davide.correndo@edu.unito.it    1104824
- Collura Federico  federico.collura@edu.unito.it   1102786

# Project 2024-2025 OPERATING SYSTEMS

## Project Structure

The project, developed as part of the university course in Operating Systems (academic year 2024/25), aims to simulate the operation of a post office using multi-process management. The project structure is organized as follows:

```
.
├── bin
│   ├── add_user.o
│   └── ...   
├── conf
│   ├── explode.conf
│   └── timeout.conf
├── include
│   └── main.h
├── src
│    ├── add_user.c
│    ├── dirctor.c
│    ├── shared_function.c
│    ├── ticket_erogator.c
│    ├── user.c
│    └── worker.c
├── makefile
├── monitor_my_program.sh
├── program_resources.log
├── readme.md
└── stats.csv

```

### Directory and File Description

- **`bin/`**: Contains configuration files required for the project.

- **`conf/`**: Contains configuration files required for the project.

  - `config_explode.conf`: Configuration for termination based on the maximum number of users waiting.
  - `config_timeout.conf`: Configuration for termination based on the maximum duration of the simulation.

- **`include/`**: Contains header (`.h`) files with shared declarations and constants.

  - `main.h`: Includes all the definitions and declarations of data structure and common functions of the project.

- **`src/`**: Contains the main source files of the project.

  - `director.c`: Implementation of the director process, responsible for the general management of the simulation, process creation, and statistics collection.
  - `add_user.c`: Module to dynamically add new users to the simulation during execution.
  - `worker.c`: Handles the operators at the service counters, including assignment and break management.
  - `ticket_erogator.c`: Implementation of the ticket distribution process for the available services.
  - `user.c`: Manages users, simulating service requests at the post office.
  - `shared_functions.c`: Common auxiliary functions used across modules.

- **`makefile`**: Script to automate the project's compilation.

## Project Objective

The objective of the project is to simulate a post office where multiple processes interact to manage users, services, and resources. The simulation includes:

- **Director Process**: Coordinates the entire simulation, creates resources and processes, and gathers statistics on the simulation's progress.
- **Ticket Distributor Process**: Provides tickets to users to access requested services.
- **Operator Process**: Manages service counters, providing specific services to users.
- **User Process**: Simulates the behavior of users requesting services.

## Requirements and Configuration

### Requirements

- Unix-like operating system.
- C compiler with support for advanced options (`-Wvla`, `-Wextra`, `-Werror`).

### Configuration

All simulation parameters are defined in the configuration files:

- **`config_explode.conf`**: Defines the maximum number of users waiting before the simulation terminates.
- **`config_timeout.conf`**: Sets the maximum duration of the simulation.

## Compilation Instructions

To compile the project, execute the following commands after cloning the repository and navigating to the main directory:

- **`make all`**: Compiles all executables required for the simulation.

## Execution

The generated executables simulate the behavior of a post office, with the ability to dynamically add new users during execution. Refer to the documentation in the `relazione.pdf` file for details on available commands.

- command execution:

```bash
./bin/director
```

- Adding new users at runtime:
  In a new shell with root set to the same directory or stopping the program with `Ctrl+z`, run the command below with `n` as the number of users to add.

```bash
./bin/add_user n
```
  if you stopped the program you can resume it with `fg`.

## Output Saving

Simulation output includes detailed statistics on system operation, automatically saved in stats.csv and printed on terminal.

## File System Cleanup

Once the simulation is complete:

- **`make clean`**: Removes object files and executables generated during compilation.
