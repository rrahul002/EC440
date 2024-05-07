#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>

#define PAGE_SIZE 4096

typedef struct { // here we define TLS (thread local storage)
    unsigned int size;
    char *data;
    int *ref_count;
} TLS;

__thread TLS *thread_tls = NULL; //thread_tls is declared here to store TLS specific to each thread and initialized to NULL
pthread_key_t tls_key;

void segfault_handler(int sig, siginfo_t *si, void *unused) {//this function is a signal handler for the segmentation faults
    if (si->si_code == SEGV_ACCERR && thread_tls != NULL) { // one such fault is if  there is an access error and thread-specific TLS data esists; if so, the thread exits.
        pthread_exit(NULL);
    } else {
        // ideally we handle other segmentation fault scenarios, but i didnt have time (ex: buffer overflow or memory corruption)
    }
}

void init_tls_key() {
    pthread_key_create(&tls_key, NULL); // here we create a pthread key to associate thread-specific data with TLS
}

int tls_create(unsigned int size) { // in this function, TLS is created for the current thread.
    if (size == 0 || thread_tls != NULL) {
        return -1;
    }
    
    TLS *tls = (TLS *)malloc(sizeof(TLS)); // we allocate memory for TLS
    if (tls == NULL) {
        return -1;
    }
    tls->size = size;
    tls->data = (char *)mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); // map the memory without permissions
    if (tls->data == MAP_FAILED) {
        free(tls);
        return -1;
    }
    tls->ref_count = (int *)malloc(sizeof(int)); //initialize the reference count
    *(tls->ref_count) = 1;
    
    pthread_setspecific(tls_key, tls); //associate TLS with the current thread and finally sets the gloabl thread TLS
    thread_tls = tls;  
    
    return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer) { // in this function, we write data to the TLS of the current thread as created in the previous function
    if (thread_tls == NULL || offset + length > thread_tls->size) { // if however, for some reason, the current thread is empty we return -1
        return -1;
    }
    
    if (mprotect(thread_tls->data + offset, length, PROT_READ | PROT_WRITE) == -1) { // we also check if the write operation is within bounds
        return -1;
    }
    
    memcpy(thread_tls->data + offset, buffer, length); // we copy data to TLS
    
    if (mprotect(thread_tls->data + offset, length, PROT_NONE) == -1) { // and protect the memory region again.
        return -1;
    }
    
    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer) { // in this function, we read data from the TLS of the current thread
    if (thread_tls == NULL || offset + length > thread_tls->size) { //similar to the write function, we ensure that the thread is not empty
        return -1;
    }
    
    if (mprotect(thread_tls->data + offset, length, PROT_READ) == -1) { // we also now allow reading from the specified memory region
        return -1;
    }
    
    memcpy(buffer, thread_tls->data + offset, length); // again we copy the necessary data from TLS
    
    if (mprotect(thread_tls->data + offset, length, PROT_NONE) == -1) { // and reprotect the memory region
        return -1;
    }
    
    return 0;
}

int tls_destroy() { // in this function, the TLS of the current thread is destroyed
    if (thread_tls == NULL) { // if the thread is null, we exit with -1
        return -1;
    }
    
    *(thread_tls->ref_count) -= 1; // we decrement by 1 in the reference count
    
    if (*(thread_tls->ref_count) == 0) { // when there are no more references, we free the memory allocated for the ref count and the thread_tls
        munmap(thread_tls->data, thread_tls->size);
        free(thread_tls->ref_count);
        free(thread_tls);
    }
    
    pthread_setspecific(tls_key, NULL); // here we disassociate TLS from the current thread.
    thread_tls = NULL;
    
    return 0;
}

int tls_clone(pthread_t tid) { //this functions close the TLS of the target thread to the current thread
    if (thread_tls != NULL) { // it first checks if the TLS of the current thread is nullified or if it exists
        return -1;
    }

    void *tls_ptr = pthread_getspecific(tls_key); //here we get the TLS of the current thread
    if (tls_ptr == NULL) {
        return -1;
    }

    TLS *target_tls = (TLS *)tls_ptr; // here we make the target_tls point to the TLS structure of the current thread
    int page_size = getpagesize();

    TLS *clone_tls = (TLS *)malloc(sizeof(TLS)); // and we allocate memory for the clone TLS
    if (clone_tls == NULL) {
        return -1;
    }
    clone_tls->size = target_tls->size; // the clone TLS is initialized here; the size
    clone_tls->data = (char *)mmap(NULL, target_tls->size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);//the data member of the clone TLS structure is initialized and sets it to read only.
    if (clone_tls->data == MAP_FAILED) {
        free(clone_tls);
        return -1;
    }
    clone_tls->ref_count = target_tls->ref_count; //this sets the ref_count of the clone TLS to the same value as the target TLS and increment the reference count
    *(clone_tls->ref_count) += 1;
    
    for (unsigned int i = 0; i < target_tls->size; i += page_size) { //this loop copies data from the target TLS to the clone TLS; it iterates over the TLS data buffer in page-sized chunks
        memcpy(clone_tls->data + i, target_tls->data + i, page_size);
    }
    
    pthread_setspecific(tls_key, clone_tls); // here we set the clone TLS as the TLS of the current thread
    thread_tls = clone_tls; //and finally we update the gloabl thread TLS to point to the clone TLS, thereby indicating that the current thread has its TLS initialized. 
    
    return 0;
}

// Additional function to handle signal actions
void register_signal_handler() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = segfault_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int main() {
    init_tls_key();
    register_signal_handler(); // Register signal handler
    
    if (tls_create(1024) == 0) {
        printf("TLS created successfully.\n");
        
        char data[] = "Hello, world!";
        if (tls_write(0, strlen(data), data) == 0) {
            printf("Data written to TLS successfully.\n");
            
            char buffer[1024];
            if (tls_read(0, sizeof(buffer), buffer) == 0) {
                printf("Data read from TLS: %s\n", buffer);
                
                if (tls_clone(pthread_self()) == 0) {
                    printf("TLS cloned successfully.\n");

                    // Test with modified data
                    char newData[] = "Modified data";
                    if (tls_write(0, strlen(newData), newData) == 0) {
                        printf("Data modified in cloned TLS successfully.\n");

                        char newBuffer[1024];
                        if (tls_read(0, sizeof(newBuffer), newBuffer) == 0) {
                            printf("Data read from cloned TLS: %s\n", newBuffer);
                        } else {
                            printf("Failed to read from cloned TLS.\n");
                        }
                    } else {
                        printf("Failed to modify data in cloned TLS.\n");
                    }
                } else {
                    printf("Failed to clone TLS.\n");
                }
                
                if (tls_destroy() == 0) {
                    printf("TLS destroyed successfully.\n");
                } else {
                    printf("Failed to destroy TLS.\n");
                }
            } else {
                printf("Failed to read from TLS.\n");
            }
        } else {
            printf("Failed to write to TLS.\n");
        }
    } else {
        printf("Failed to create TLS.\n");
    }
    
    return 0;
}
