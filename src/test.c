#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "main.h"

// Mock functions to simulate IPC behavior
static int mock_semid = 1;
static int mock_shmid = 1;
static int mock_msgid = 1;
static int mock_sem_values[100] = {0};
static worker_seat mock_seats[100] = {0};
static daily_stats mock_daily_stats[100] = {0};
static tot_stats mock_tot_stats = {0};
static int mock_macros[NUM_MACROS] = {0};

// Mock IPC functions
int mock_semget(key_t key, int nsems, int semflg) {
    return mock_semid;
}

int mock_shmget(key_t key, size_t size, int shmflg) {
    return mock_shmid;
}

void* mock_shmat(int shmid, const void *shmaddr, int shmflg) {
    if (shmid == mock_shmid) {
        switch(shmid) {
            case 1: return mock_seats;
            case 2: return &mock_daily_stats;
            case 3: return &mock_tot_stats;
            case 4: return mock_macros;
            default: return NULL;
        }
    }
    return NULL;
}

// Test Helper Functions
void reset_mock_data() {
    memset(mock_sem_values, 0, sizeof(mock_sem_values));
    memset(mock_seats, 0, sizeof(mock_seats));
    memset(&mock_daily_stats, 0, sizeof(mock_daily_stats));
    memset(&mock_tot_stats, 0, sizeof(mock_tot_stats));
    memset(mock_macros, 0, sizeof(mock_macros));
}

// Test Cases

void test_find_seat() {
    printf("Testing find_seat function...\n");
    
    // Setup test data
    mock_macros[1] = 5; // Number of seats
    worker_seat seats[5] = {
        {0, 1, false, 0},
        {1, 2, true, 1},
        {2, 1, false, 0},
        {3, 3, false, 0},
        {4, 1, true, 2}
    };
    memcpy(mock_seats, seats, sizeof(seats));
    
    // Test 1: Finding available seat for task 1
    assert(find_seat(mock_seats, mock_macros, 1, 3, mock_semid) == true);
    printf("Test 1 passed: Found available seat for task 1\n");
    
    // Test 2: Finding seat for non-existent task
    assert(find_seat(mock_seats, mock_macros, 7, 3, mock_semid) == false);
    printf("Test 2 passed: No seat found for non-existent task\n");
    
    // Test 3: All seats for task occupied
    mock_seats[2].busy = true;
    assert(find_seat(mock_seats, mock_macros, 1, 3, mock_semid) == false);
    printf("Test 3 passed: No available seats when all are occupied\n");
    
    reset_mock_data();
}

void test_worker_per_task() {
    printf("Testing worker_per_task function...\n");
    
    // Setup test data
    mock_macros[0] = 5; // Number of workers
    worker_seat seats[5] = {
        {0, 1, false, 1},
        {1, 1, false, 2},
        {2, 2, false, 3},
        {3, 1, false, 4},
        {4, 3, false, 5}
    };
    memcpy(mock_seats, seats, sizeof(seats));
    
    // Test 1: Count workers for task 1
    int result = worker_per_task(mock_macros, mock_seats, 1);
    assert(result == 3);
    printf("Test 1 passed: Correctly counted 3 workers for task 1\n");
    
    // Test 2: Count workers for task 2
    result = worker_per_task(mock_macros, mock_seats, 2);
    assert(result == 1);
    printf("Test 2 passed: Correctly counted 1 worker for task 2\n");
    
    // Test 3: Count workers for non-existent task
    result = worker_per_task(mock_macros, mock_seats, 5);
    assert(result == 0);
    printf("Test 3 passed: Correctly counted 0 workers for non-existent task\n");
    
    reset_mock_data();
}

void test_ratio_worker_seats() {
    printf("Testing ratio_worker_seats function...\n");
    
    // Setup test data
    mock_macros[0] = 6; // Number of workers
    mock_macros[1] = 4; // Number of seats
    worker_seat seats[4] = {
        {0, 1, false, 1},
        {1, 1, false, 2},
        {2, 2, false, 3},
        {3, 1, false, 4}
    };
    memcpy(mock_seats, seats, sizeof(seats));
    
    // Test 1: Calculate ratio for task 1 seats
    float ratio = ratio_worker_seats(mock_macros, mock_seats, 0);
    assert(ratio > 0.99 && ratio < 1.01); // Allow for floating point comparison
    printf("Test 1 passed: Correct ratio calculation for task 1\n");
    
    // Test 2: Calculate ratio for task 2 seat
    ratio = ratio_worker_seats(mock_macros, mock_seats, 2);
    assert(ratio > 0.99 && ratio < 1.01);
    printf("Test 2 passed: Correct ratio calculation for task 2\n");
    
    reset_mock_data();
}

void test_update_stats() {
    printf("Testing update_stats function...\n");
    
    // Setup test data
    int day = 0;
    mock_macros[0] = 5; // Number of workers
    mock_macros[1] = 3; // Number of seats
    int task = 1;
    int user_served = 10;
    int task_time = 100;
    bool pause = true;
    int wait_time = 50;
    
    // Test 1: Basic stats update
    update_stats(&mock_daily_stats[0], &mock_tot_stats, mock_semid, day, 
                mock_macros, task, mock_seats, user_served, task_time, 
                pause, wait_time);
    
    assert(mock_tot_stats.wait_time == wait_time);
    assert(mock_tot_stats.task_time == task_time);
    assert(mock_tot_stats.num_user_served == user_served);
    assert(mock_daily_stats[day].user_not_served_daily == user_served);
    assert(mock_daily_stats[day].daily_waiting_time == wait_time);
    printf("Test 1 passed: Basic stats updated correctly\n");
    
    // Test 2: Cumulative stats update
    update_stats(&mock_daily_stats[0], &mock_tot_stats, mock_semid, day, 
                mock_macros, task, mock_seats, user_served, task_time, 
                pause, wait_time);
    
    assert(mock_tot_stats.wait_time == wait_time * 2);
    assert(mock_tot_stats.task_time == task_time * 2);
    assert(mock_tot_stats.num_user_served == user_served * 2);
    printf("Test 2 passed: Cumulative stats updated correctly\n");
    
    reset_mock_data();
}

int main() {
    printf("Starting unit tests...\n\n");
    
    test_find_seat();
    test_worker_per_task();
    test_ratio_worker_seats();
    test_update_stats();
    
    printf("\nAll tests completed successfully!\n");
    return 0;
}