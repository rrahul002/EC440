// threading.c

#include "threading.h"  // Including the header file for function prototypes
#include <stdlib.h>     // Including standard library headers
#include <signal.h>     // Including signal handling headers

// Initialize a mutex
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr) {
    if (mutex == NULL)  // Check if mutex pointer is NULL
        return EINVAL;  // Return EINVAL if it is

    // Initialize the mutex using PTHREAD_MUTEX_INITIALIZER macro
    *mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    return 0;  // Return 0 to indicate success
}

// Destroy a mutex
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    if (mutex == NULL || *mutex == (pthread_mutex_t)NULL) {  // Check if mutex is NULL or already destroyed
        return EINVAL;  // Return EINVAL if it is
    }

    *mutex = (pthread_mutex_t)NULL;  // Set mutex to NULL
    return 0;  // Return 0 to indicate success
}

// Lock a mutex
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (mutex == NULL || *mutex == (pthread_mutex_t)NULL) {  // Check if mutex is NULL or already destroyed
        return EINVAL;  // Return EINVAL if it is
    }

    // Lock the mutex using pthread_mutex_lock
    if (pthread_mutex_lock(mutex) != 0) {
        return -1;  // Return -1 if locking fails
    }

    return 0;  // Return 0 to indicate success
}

// Unlock a mutex
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (mutex == NULL || *mutex == (pthread_mutex_t)NULL) {  // Check if mutex is NULL or already destroyed
        return EINVAL;  // Return EINVAL if it is
    }

    // Unlock the mutex using pthread_mutex_unlock
    if (pthread_mutex_unlock(mutex) != 0) {
        return -1;  // Return -1 if unlocking fails
    }

    return 0;  // Return 0 to indicate success
}

// Initialize a barrier
int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count) {
    if (barrier == NULL || count == 0)  // Check if barrier pointer is NULL or count is 0
        return EINVAL;  // Return EINVAL if either condition is true

    // Allocate memory for the barrier data structure
    barrier->__data = (struct __pthread_barrier_internal *)malloc(sizeof(struct __pthread_barrier_internal));
    if (barrier->__data == NULL)  // Check if memory allocation fails
        return ENOMEM;  // Return ENOMEM if it does

    barrier->__data->__count = count;  // Set the count in the barrier data structure
    barrier->__data->__waiting = 0;    // Set the waiting count to 0

    return 0;  // Return 0 to indicate success
}

// Destroy a barrier
int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    if (barrier == NULL || barrier->__data == NULL) {  // Check if barrier pointer or data is NULL
        return EINVAL;  // Return EINVAL if either condition is true
    }

    free(barrier->__data);  // Free memory allocated for the barrier data structure
    barrier->__data = NULL; // Set barrier data pointer to NULL

    return 0;  // Return 0 to indicate success
}

// Wait on a barrier
int pthread_barrier_wait(pthread_barrier_t *barrier) {
    if (barrier == NULL || barrier->__data == NULL) {  // Check if barrier pointer or data is NULL
        return EINVAL;  // Return EINVAL if either condition is true
    }

    // Need to finish barrier wait

    return 0;  // Return 0 to indicate success
}

// Helper function to lock signals
static void lock() {
    sigset_t mask;          // Define a signal set
    sigemptyset(&mask);     // Initialize the signal set to empty
    sigaddset(&mask, SIGALRM);  // Add SIGALRM to the signal set
    sigprocmask(SIG_BLOCK, &mask, NULL);  // Block signals in the process using the signal set
}

// Helper function to unlock signals
static void unlock() {
    sigset_t mask;          // Define a signal set
    sigemptyset(&mask);     // Initialize the signal set to empty
    sigaddset(&mask, SIGALRM);  // Add SIGALRM to the signal set
    sigprocmask(SIG_UNBLOCK, &mask, NULL);  // Unblock signals in the process using the signal set
}
