//gcc -Wall -Werror a2.c a2_helper.c -o a2 -lpthread
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include "a2_helper.h"
#include <stdbool.h>

sem_t p7_sem_1, p7_sem_2, p2_sem_1, p2_sem_2, p2_sem_3;
sem_t *p4_sem_1, *p4_sem_2, *p4_sem_3; // These will be named semaphores for inter-process operations

#define SEM_NAME1 "/my_semaphore1"
#define SEM_NAME2 "/my_semaphore2"
#define SEM_NAME3 "/my_semaphore3"

int no_threads_waiting_p2 = 0;
pthread_mutex_t mutex_p2;

void p7_thread(void *arg) {
            
    int th_id = *((int*)arg);

    if(th_id == 2) {
        //sem_post(p4_sem_2); // Allow T4.5 to terminate first but wait until it terminated before starting
        sem_wait(p4_sem_3);
    }

    if(th_id == 3) {
        sem_wait(&p7_sem_1); // Block starting of T3 until T1 started first
    }
    info(BEGIN, 7, th_id);
    if(th_id == 1) {
        sem_post(&p7_sem_1); // Allow T3 to start now that T1 started first
    }

    if(th_id == 1) { // Block T1 until T3 terminates
        sem_wait(&p7_sem_2);
    }

    info(END, 7, th_id);

    if(th_id == 3) { // After T3 terminates, unbock T1
        sem_post(&p7_sem_2);
    }

    if(th_id == 2) {
        sem_post(p4_sem_1); // After T7.2 terminates, only then allow T4.2 to start
    }
}

void p4_thread(void *arg) {
            
    int th_id = *((int*)arg);

    if(th_id == 2) { // Block T4.2 from starting until T7.2 terminates first
        sem_wait(p4_sem_1);
    }

    info(BEGIN, 4, th_id);
    
    // if(th_id == 5) { // Block T4.5 from terminating until T7.2 starts first
    //     sem_wait(p4_sem_2);
    // }

    info(END, 4, th_id);

    if(th_id == 5) {
        sem_post(p4_sem_3); // Allow T7.2 to start now that T4.5 terminated first
    }
    
}

void p2_thread(void *arg) {
            
    int th_id = *((int*)arg);

    sem_wait(&p2_sem_1); // Decrement semaphore for each thread that attempts to run.
    info(BEGIN, 2, th_id);
    
    if(th_id != 10) {
        if(sem_trywait(&p2_sem_3) == -1) {// 37 threads other than T10 will terminate, the other 5 will wait
            pthread_mutex_lock(&mutex_p2); // Protect global varible from concurrent modification
            ++no_threads_waiting_p2;
            //printf("Thread %d is waiting...\n", th_id);
            if(no_threads_waiting_p2 == 5) {
                sem_post(&p2_sem_2); // Unblock T10 to allow it to terminate
            }
            pthread_mutex_unlock(&mutex_p2);
            sem_wait(&p2_sem_3);
        }
    }

    if(th_id == 10) {
        sem_wait(&p2_sem_2); // Block thread 10 from terminating until condition is met
    }

    info(END, 2, th_id);
    sem_post(&p2_sem_1); // Increment semaphore after a thread finishes running

    if(th_id == 10) {
        for(int i = 0; i < 5; ++i) {
            sem_post(&p2_sem_3); // Now T10 will release other 5 waiting threads to allow them to terminate as well
        }
    }
}

int main(){
    init();
    
    pid_t pid2, pid3, pid4, pid5, pid6, pid7;

    info(BEGIN, 1, 0);

    // Initialize special named semaphores for inter-process communication
    sem_unlink(SEM_NAME1);
    sem_unlink(SEM_NAME2);
    sem_unlink(SEM_NAME3);
    p4_sem_1 = sem_open(SEM_NAME1, O_CREAT | O_EXCL, 0666, 0);
    if(p4_sem_1 == SEM_FAILED) {
        perror("Cannot create sem1_p4");
        exit(-1);
    }
    p4_sem_2 = sem_open(SEM_NAME2, O_CREAT | O_EXCL, 0666, 0);
    if(p4_sem_2 == SEM_FAILED) {
        perror("Cannot create sem2_p4");
        exit(-1);
    }
    p4_sem_3 = sem_open(SEM_NAME3, O_CREAT | O_EXCL, 0666, 0);
    if(p4_sem_3 == SEM_FAILED) {
        perror("Cannot create sem3_p4");
        exit(-1);
    }

    pid2 = fork();
    if (pid2 == 0) {
        info(BEGIN, 2, 0);

        pid7 = fork();
        if (pid7 == 0) {
            info(BEGIN, 7, 0);

            pthread_t threads_p7[4];
            int th_ids_p7[4];
            sem_init(&p7_sem_1, 0, 0); // Initialize the semaphore with value 0
            sem_init(&p7_sem_2, 0, 0); // Initialize the semaphore with value 0

            //Create 4 threads for p7
            for(int i = 0; i < 4; ++i) {
                th_ids_p7[i] = i + 1; // Threads start from 1, not 0
                pthread_create(&threads_p7[i], NULL, (void*) p7_thread, &th_ids_p7[i]);
            }

            // Wait for all threads to finish execution before terminating main thread
            for(int i = 0; i < 4; ++i) {
                pthread_join(threads_p7[i], NULL);
            }

            sem_destroy(&p7_sem_1); // Destroy the semaphore
            sem_destroy(&p7_sem_2); // Destroy the semaphore
            info(END, 7, 0);
            return 0;
        }
        else {
            wait(NULL); // P2 waits for P7
        }

        pthread_t threads_p2[42];
        int th_ids_p2[42];
        sem_init(&p2_sem_1, 0, 6); // Initialize the semaphore with value 6
        sem_init(&p2_sem_2, 0, 0); // Initialize the semaphore with value 0
        sem_init(&p2_sem_3, 0, 36); // Initialize the semaphore with value 37
        pthread_mutex_init(&mutex_p2, NULL); // Initialize mutex lock

        //Create 42 threads for p2
        for(int i = 0; i < 42; ++i) {
            th_ids_p2[i] = i + 1; // Threads start from 1, not 0
            pthread_create(&threads_p2[i], NULL, (void*) p2_thread, &th_ids_p2[i]);
        }

        // Wait for all threads to finish execution before terminating main thread
        for(int i = 0; i < 42; ++i) {
            pthread_join(threads_p2[i], NULL);
        }

        pthread_mutex_destroy(&mutex_p2); // Destroy the mutex lock
        sem_destroy(&p2_sem_1); // Destroy the semaphore
        sem_destroy(&p2_sem_2); // Destroy the semaphore
        sem_destroy(&p2_sem_3); // Destroy the semaphore

        info(END, 2, 0);
        return 0;

    }
    else {
        //wait(NULL); // P1 waits for P2
        pid3 = fork();
        if (pid3 == 0) {
            info(BEGIN, 3, 0);

            pid4 = fork();
            if (pid4 == 0) {
                info(BEGIN, 4, 0);

                pid5 = fork();
                if (pid5 == 0) {
                    info(BEGIN, 5, 0);

                    pid6 = fork();
                if (pid6 == 0) {
                    info(BEGIN, 6, 0);
                    info(END, 6, 0);
                    return 0;
                }
                else {
                    wait(NULL); // P5 waits for P6
                }

                    info(END, 5, 0);
                    return 0;
                }
                else {
                    wait(NULL); // P4 waits for P5
                }
                
                pthread_t threads_p4[6];
                int th_ids_p4[6];

                //Create 6 threads for p4
                for(int i = 0; i < 6; ++i) {
                    th_ids_p4[i] = i + 1; // Threads start from 1, not 0
                    pthread_create(&threads_p4[i], NULL, (void*) p4_thread, &th_ids_p4[i]);
                }

                // Wait for all threads to finish execution before terminating main thread
                for(int i = 0; i < 6; ++i) {
                    pthread_join(threads_p4[i], NULL);
                }

                info(END, 4, 0);
                return 0;
            }
            else {
                wait(NULL); // P3 waits for P4
            }

            info(END, 3, 0);
            return 0;
        }
        else {
            wait(NULL); // P1 waits for P3
        }

        wait(NULL); // P1 waits for P2

        sem_close(p4_sem_1);
        sem_close(p4_sem_2);
        sem_close(p4_sem_3);

        sem_unlink(SEM_NAME1);
        sem_unlink(SEM_NAME2);
        sem_unlink(SEM_NAME3);

        info(END, 1, 0);
        return 0;
    }
}
