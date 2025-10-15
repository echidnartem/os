#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h> 

static char PROCESSOR_1[] = "processor1";
static char PROCESSOR_2[] = "processor2";

int main(int argc, char * argv[]) {
    if (argc == 1) {
        char message[512];
        int message_length = snprintf(message, sizeof(message), "Usage: %s <filename>\n", argv[0]);
        write(STDERR_FILENO, message, message_length);
        exit(EXIT_FAILURE); 
    }



    char program_path[1024];

    ssize_t path_length = readlink("/proc/self/exe", program_path, sizeof(program_path) - 1);
    if (path_length == 0) {
        const char error[] = "Error: Failed to read program path\n";
        write(STDERR_FILENO, error, sizeof(error));
        exit(EXIT_FAILURE);
    }

    while (path_length > 0 && program_path[path_length] != '\\' && program_path[path_length] != '/') {
        --path_length;
    }
    program_path[path_length] = '\0';



    int pipe_client_to_1[2];
    int pipe_1_to_2[2];
    int pipe_2_to_client[2];

    if (pipe(pipe_client_to_1) == -1) {
        const char error[] = "Error: Failed to create pipe [client to processor 1]\n";
        write(STDERR_FILENO, error, sizeof(error));
        exit(EXIT_FAILURE);
    }

    if (pipe(pipe_1_to_2) == -1) {
        const char error[] = "Error: Failed to create pipe [processor 1 to processor 2]\n";
        write(STDERR_FILENO, error, sizeof(error));
        exit(EXIT_FAILURE);
    }

    if (pipe(pipe_2_to_client) == -1) {
        const char error[] = "Error: Failed to create pipe [processor 2 to client]\n";
        write(STDERR_FILENO, error, sizeof(error));
        exit(EXIT_FAILURE);
    }



    const pid_t child1 = fork();
    switch (child1) {
        case -1: {
            const char error[] = "Error: Failed to spawn new process\n";
            write(STDERR_FILENO, error, sizeof(error));
            exit(EXIT_FAILURE);
        } break;

        case 0: {
            close(pipe_2_to_client[0]);
            close(pipe_2_to_client[1]);

            close(pipe_client_to_1[1]);
            close(pipe_1_to_2[0]);

            dup2(pipe_client_to_1[0], STDIN_FILENO);
            close(pipe_client_to_1[0]); 

            dup2(pipe_1_to_2[1], STDOUT_FILENO);
            close(pipe_1_to_2[1]);

            char path[2048];
            snprintf(path, sizeof(path), "%s/%s", program_path, PROCESSOR_1);

            char * const args[] = {PROCESSOR_1, argv[1], NULL};
   
            int status = execv(path, args);

            if (status == -1) {
                const char error[] = "Error: Failed to execute new executable image\n";
                write(STDERR_FILENO, error, sizeof(error));
                exit(EXIT_FAILURE);
            }
        } break;

        default: {
            const pid_t child2 = fork();

            switch (child2) {
                case -1: {
                    const char error[] = "Error: Failed to spawn new process\n";
                    write(STDERR_FILENO, error, sizeof(error));
                    exit(EXIT_FAILURE);
                } break;

                case 0: {
                    close(pipe_client_to_1[0]);
                    close(pipe_client_to_1[1]);

                    close(pipe_1_to_2[1]);
                    close(pipe_2_to_client[0]);

                    dup2(pipe_1_to_2[0], STDIN_FILENO);
                    close(pipe_1_to_2[0]);

                    dup2(pipe_2_to_client[1], STDOUT_FILENO);
                    close(pipe_2_to_client[1]);

                    char path[2048];
                    snprintf(path, sizeof(path), "%s/%s", program_path, PROCESSOR_2);

                    char * const args[] = {PROCESSOR_2, argv[1], NULL};

                    int status = execv(path, args);

                    if (status == -1) {
                        const char error[] = "Error: Failed to execute new executable image\n";
                        write(STDERR_FILENO, error, sizeof(error));
                        exit(EXIT_FAILURE);
                    }
                } break;

                default: {
                    pid_t pid = getpid();
                    {
                        char message[256];

                        const int length = snprintf(
                            message, sizeof(message), 
                            "PID %d: I`m a parent, my child 1 has PID %d, child 2 has PID %d.\n           To exit, press CTRL+C\n", 
                            pid, child1, child2);
                        write(STDOUT_FILENO, message, length);
                    }

                    close(pipe_client_to_1[0]);     

                    close(pipe_1_to_2[0]);
                    close(pipe_1_to_2[1]);
                    
                    close(pipe_2_to_client[1]);

                    char buffer[4048];
                    ssize_t bytes;

                    char message[1024];
                    const int length = snprintf(message, sizeof(message),
                        "PID %d: Write a message that needs to be changed: ", pid);
                    write(STDOUT_FILENO, message, length);

                    while (bytes = read(STDIN_FILENO, buffer, sizeof(buffer))) {
                        if (bytes < 0) {
                            const char message[] = "Error: Failed to read stdin\n";
                            write(STDERR_FILENO, message, sizeof(message));
                            exit(EXIT_FAILURE);
                        } else if (buffer[0] == '\n') break;

                        write(pipe_client_to_1[1], buffer, bytes);



                        bytes = read(pipe_2_to_client[0], buffer, bytes);
                        char info_pid[32];
                        int length_info_pid = snprintf(info_pid, sizeof(info_pid), "PID %d: ", pid);

                        write(STDOUT_FILENO, info_pid, length_info_pid);
                        write(STDOUT_FILENO, buffer, bytes);
                        write(STDOUT_FILENO, message, length);
                    }

                    close(pipe_client_to_1[1]);
                    close(pipe_2_to_client[0]);

                    waitpid(child1, NULL, 0);
                    waitpid(child2, NULL, 0);
                } break;
            }            
        } break;
    }
    return 0;
}
