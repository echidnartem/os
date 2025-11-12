#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <string.h>
#include <wait.h>
#include <stdio.h>


#define SHM_SIZE 4096


char SHM_NAME[1024] = "shm-name";
char SEM_CLIENT[1024] = "sem-client";
char SEM_PROCESSOR_1[1024] = "sem-processor1";
char SEM_PROCESSOR_2[1024] = "sem-processor2";


int main() {
    char unique_suffix[64] = "\0";
    snprintf(unique_suffix, sizeof(unique_suffix), "%d", getpid());



    snprintf(SHM_NAME, sizeof(SHM_NAME), "/shm-name_%s", unique_suffix);
    snprintf(SEM_CLIENT, sizeof(SEM_CLIENT), "/sem-client_%s", unique_suffix);
    snprintf(SEM_PROCESSOR_1, sizeof(SEM_PROCESSOR_1), "/sem-processor1_%s", unique_suffix);
    snprintf(SEM_PROCESSOR_2, sizeof(SEM_PROCESSOR_2), "/sem-processor2_%s", unique_suffix);



    int shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (shm < 0) {
        const char error[] = "Error: Failed to open SHM\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    if (ftruncate(shm, SHM_SIZE) < 0) {
        const char error[] = "Error: Failed to resize SHM\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    char * shm_buffer = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
    if (shm_buffer == MAP_FAILED) {                            // отображает в виртуальное адресное пространство процесса, чтобы     
        const char error[] = "Error: Failed to map SHM\n";     // можно было работать как с обычным участком памяти (через указатель)
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }



    sem_t * sem_wait_for_processor2 = sem_open(SEM_CLIENT, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
    if (sem_wait_for_processor2 == SEM_FAILED) {
        const char error[] = "Error: Failed to create semaphore (client)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    sem_t * sem_signal_to_processor1 = sem_open(SEM_PROCESSOR_1, O_RDWR | O_CREAT | O_TRUNC, 0600, 0);
    if (sem_signal_to_processor1 == SEM_FAILED) {
        const char error[] = "Error: Failed to create semaphore (processor 1)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    sem_t * sem_processor_2 = sem_open(SEM_PROCESSOR_2, O_RDWR | O_CREAT | O_TRUNC, 0600, 0);
    if (sem_processor_2 == SEM_FAILED) {
        const char error[] = "Error: Failed to create semaphore (processor 2)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }



    pid_t child1 = fork();
    if (child1 == 0) {
        char * args[] = {"processor1", SHM_NAME, SEM_CLIENT, SEM_PROCESSOR_1, SEM_PROCESSOR_2, NULL};
        execv("./processor1", args);

        const char error[] = "Error: Failed to exec (processor 1)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    } else if (child1 == -1) {
        const char error[] = "Error: Failed to fork (processor 1)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }



    pid_t child2 = fork();
    if (child2 == 0) {
        char * args[] = {"processor2", SHM_NAME, SEM_CLIENT, SEM_PROCESSOR_1, SEM_PROCESSOR_2, NULL};
        execv("./processor2", args);

        const char error[] = "Error: failed to exec (processor 2)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    } else if (child2 == -1) {
        const char error[] = "Error: Failed to fork (processor 2)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }



    bool running = true;
    while (running) {
        if (sem_wait(sem_wait_for_processor2) < 0) {
            const char error[] = "Error: sem_wait failed\n";
            write(STDERR_FILENO, error, sizeof(error));
            _exit(EXIT_FAILURE);
        }

        int * length = (int *)shm_buffer;
        char * text = shm_buffer + sizeof(int);

        if (*length == -1) {
            running = false;
        } else if (*length > 0) {                              // длина изменилась => это результат работы processor2                         
            const char message[] = "Result: ";
            write(STDOUT_FILENO, message, sizeof(message) - 1);
            write(STDOUT_FILENO, text, *length);

            *length = 0;
            sem_post(sem_wait_for_processor2);
        } else {                                               // длина 0 => данных нет => надо получить их от пользователя
            const char message[] = "Enter text (Ctrl+D for exit): ";
            write(STDOUT_FILENO, message, sizeof(message) - 1); 

            char buf[SHM_SIZE - sizeof(int)];
            ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf));

            if (bytes < 0) {
                const char error[] = "Error: Failed to read from standart input\n";
                write(STDERR_FILENO, error, sizeof(error));
                _exit(EXIT_FAILURE);
            }

            if (bytes > 0) {
                *length = bytes;
                memcpy(text, buf, bytes);
                sem_post(sem_signal_to_processor1);
            } else {
                *length = -1;
                running = false;
                sem_post(sem_signal_to_processor1);
            }
        }
    }

    waitpid(child1, NULL, 0);
    waitpid(child2, NULL, 0);

    sem_unlink(SEM_CLIENT);
    sem_unlink(SEM_PROCESSOR_1);
    sem_unlink(SEM_PROCESSOR_2);

    sem_close(sem_wait_for_processor2);
    sem_close(sem_signal_to_processor1);
    sem_close(sem_processor_2);

    munmap(shm_buffer, SHM_SIZE);
    shm_unlink(SHM_NAME);
    close(shm);

    return 0;
}