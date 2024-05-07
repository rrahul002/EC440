//header files as included from orginal file
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <assert.h>
#include "ec440threads.h"


#define MAX_THREADS 128			/* number of threads you support */
#define THREAD_STACK_SIZE (1<<15)	/* size of stack in bytes */
#define QUANTUM (50 * 1000)		/* quantum in usec */

/* 
   Thread_status identifies the current state of a thread. What states could a thread be in?
   Values below are just examples you can use or not use. 
 */
enum thread_status
{
 TS_EXITED,
 TS_RUNNING,
 TS_READY
};

/* The thread control block stores information about a thread. You will
 * need one of this per thread. What information do you need in it? 
 * Hint, remember what information Linux maintains for each task?
 */
struct thread_control_block {
	jmp_buf buffer;
	bool exited;
	enum thread_status status;
	void *stack;
	void *exit_status;
};

static struct thread_control_block threads[MAX_THREADS];
static pthread_t next_thread_id = 0;
static pthread_t current_thread_id = 0;

static void schedule(int signal) { //this is the scheduler function, it should determine what thread to run next when the timer signal is recieved. 
    if (setjmp(threads[current_thread_id].buffer) == 0) { //here i use setjmp to save the execution context of each thread before switching to another thread; however if setjmp returns a 0, it means its the first time the thread is running and the scheduler needs to set it up.
        if (threads[current_thread_id].status != TS_EXITED) { //here i just check if the current thread hasnt exited and therefore ready to run.
        	threads[current_thread_id].status = TS_READY; //Now that we know that our thread is unscheduled (for now) and hasn't exited either, we mark it ready to run.
    	    save_thread_state(&threads[current_thread_id]);//this state is saved using the save_thread_state function
        
            pthread_t next_thread_id = (current_thread_id + 1) % MAX_THREADS; //here we implement our round-robbin technique to find the next thread ready to run.
            while (threads[next_thread_id].status == TS_EXITED) { //we iterate thru each thread in the thread array until we find one that has not been marked exited.
                next_thread_id = (next_thread_id + 1) % MAX_THREADS;
        	}
            
            threads[next_thread_id].status = TS_RUNNING; //we mark our new thread as running
            restore_thread_state(&threads[next_thread_id]);
            current_thread_id = next_thread_id;//update the current thread ID
            longjmp(threads[next_thread_id].buffer, 1); //and we finally jump to the next thread's saved execution context with longjmp, which is stored in the jmp_buf associated with threads[next_thread_id].buffer
        }
    }
}

// to supress compiler error saying these static functions may not be used...
static void schedule(int signal) __attribute__((unused));

//in this part, we intialize the thread scheduler, this part entails setting up a timer for the schedule function
static void scheduler_init() {
    struct sigaction sa; //these are the variables that will handle the signals for the scheduling
    struct itimerval timer;
    
    sa.sa_handler = &schedule; //we set up the schedule for the SIGALRM signal
    sa.sa_flags = SA_NODEFER; //this flag allows for multiple signals to be processed concurrently
    sigemptyset(&sa.sa_mask);//  and therefore the signal cant be blocked while handling
    sigaction(SIGALRM, &sa, NULL); //here we set up the schedule function be called whenever the SIGALRM signal is recieved.
    
    timer.it_value.tv_sec = 0; //here we set up the timer 
    timer.it_value.tv_usec = QUANTUM; //the timer is now set to send signals at regular intervals
    timer.it_interval = timer.it_value; //we configure the time to repeat with the same interval after each expiration
    setitimer(ITIMER_REAL, &timer, NULL);//SET the real-time timer to use the 'timer' specified values.
    
    // Initialize global threading data structures
    // Create a Thread Control Block (TCB) for the main thread
    threads[0].exited = false;
    threads[0].status = TS_RUNNING;
    threads[0].stack = NULL; // Assuming the main thread doesn't need a separate stack
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                   void *(*start_routine)(void *), void *arg) { //this function creates a new thread within a process; pthread_t holds the ID of the new created thread.
    static bool is_first_call = true; //here we make sure that the scheduler is only initialized once.
    if (is_first_call) {
        is_first_call = false;
        scheduler_init();
    }
    
    if (next_thread_id >= MAX_THREADS) { //here is a simple check to see if the max number of threads has been reached, or else -1 is returned
        return -1;
    }
    
    void *stack = malloc(THREAD_STACK_SIZE); //here we dynamically allocate memory for the new thread stack and if this fails, -1 is returned
    if (stack == NULL) {
        return -1;
    }
    
    struct thread_control_block *tcb = &threads[next_thread_id++]; // a TCB is created for the new thread; the exited flag is cleared, the status is made ready
    tcb->exited = false;
    tcb->status = TS_READY;
    tcb->stack = stack;
    
    if (setjmp(tcb->buffer) == 0) { //here setjmp saved the current execution context of the new thread along with the stack frame to buffer. if this is 0, this is an initial call; in other words a new thread.
        unsigned long int sp = (unsigned long int)stack + THREAD_STACK_SIZE - 8; //here we calculate the stack pointer for the new thread; we subtract 8 because the stack grows downwards and the top of the stack is actually 8 bytes below the end of the allocated memory.
        unsigned long int pc = (unsigned long int)start_routine; //this line calculated the program counter for the new thread to ensure compatibility with size of memory
        tcb->buffer->__jmpbuf[6] = sp; //we assign the calculated stack pointer to the 6th element of the __jmpbuf which contains the saved state of the thread's registers and execution context.
        tcb->buffer->__jmpbuf[7] = pc;//similarly the program counter is assigned to the 7th element
        set_reg(&tcb->buffer, JB_R12, (unsigned long int)start_routine); //here the JB_R12, which is used to pass the address of the fuction to be called, to store the address of the start_routine function, whih wil be the entry point for the new thread's execution
        set_reg(&tcb->buffer, JB_R13, (unsigned long int)arg);//and here the arg passed to that start_routine function is registered
    } else {
        pthread_exit(tcb->exit_status); //however if setjmp returns a non-zero, our selected thread is returning from a longjmp and the thread is exiting, so we assign it the exiting state and we use pthread_exit to clean our resources.
    }
    
    *thread = next_thread_id - 1; //we finally store the new thread's ID in the location pointed to by thread.
    
    return 0;
}

void pthread_exit(void *value_ptr) { // this function is used to exit the current thread
    free(threads[current_thread_id].stack); //first we freee the memory allocated for the stack of the current thread.
    threads[current_thread_id].exited = true;// the exited flag is attached to the current thread
    threads[current_thread_id].exit_status = value_ptr;// this part is useful for pthread_join to retrieve
    
    // Call scheduler to switch to another thread
    schedule(SIGALRM);
}

pthread_t pthread_self(void) {
    return current_thread_id; //this function sumply returns the id of the current thread
}

int pthread_join(pthread_t thread, void **retval) {// this function waits for the specified thread to exit and retrieves its exit status
    while (!threads[thread].exited) { 
        // Wait for the target thread to exit
    }
    
    if (retval != NULL) {
        *retval = threads[thread].exit_status; //this just double checks if the thread is in the exit status, if not the exit status is assigned.
    }
    
    return 0;
}