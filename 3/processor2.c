#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <semaphore.h>


#define SHM_SIZE 4096


int main(int argc, char * argv[]) {
    if (argc < 5) {
        const char error[] = "Error: Not enough arguments\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }


    const char * SHM_NAME = argv[1];
    const char * SEM_CLIENT = argv[2];
    const char * SEM_PROCESSOR_1 = argv[3];
    const char * SEM_PROCESSOR_2 = argv[4];


    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0600);
    if (shm_fd < 0) {
        const char error[] = "Error: Failed to open SHM\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    char * shm_buffer = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_buffer == MAP_FAILED) {
        const char error[] = "Error: Failed to map shared memory\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    sem_t * sem_wait_for_processor1 = sem_open(SEM_PROCESSOR_2, O_RDWR);
    if (sem_wait_for_processor1 == SEM_FAILED) {
        const char error[] = "Error: Failed to open semaphore (processor 2)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    sem_t * sem_signal_to_client = sem_open(SEM_CLIENT, O_RDWR);
    if (sem_signal_to_client == SEM_FAILED) {
        const char error[] = "Error: Failed to open semaphore (client)\n";
        write(STDERR_FILENO, error, sizeof(error));
        _exit(EXIT_FAILURE);
    }

    bool running = true;
    while (running) {
        if (sem_wait(sem_wait_for_processor1) < 0) {
            const char error[] = "Error: sem_wait failed\n";
            write(STDERR_FILENO, error, sizeof(error));
            _exit(EXIT_FAILURE);
        }

        int * length = (int *)shm_buffer;
        char * text = shm_buffer + sizeof(int);

        if (*length == -1) {
            running = false;
        } else if (*length > 0) {
            int new_length = 0;
            bool prev_is_space = 0;
            for (int i = 0; i < *length; ++i) {
                if (text[i] == ' ') {
                    if (!prev_is_space) {
                        text[new_length++] = text[i];
                        prev_is_space = true;
                    }
                }
                else {
                    text[new_length++] = text[i];
                    prev_is_space = false;
                }
            }

            *length = new_length;
        }

        if (sem_post(sem_signal_to_client) < 0) {
            const char error[] = "Error: sem_post failed\n";
            write(STDERR_FILENO, error, sizeof(error));
            _exit(EXIT_FAILURE);
        }
    }

    sem_close(sem_wait_for_processor1);
    sem_close(sem_signal_to_client);

    munmap(shm_buffer, SHM_SIZE);
    close(shm_fd);

    return 0;
}