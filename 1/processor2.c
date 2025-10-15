#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char * argv[]) {
    pid_t pid = getpid();
    char buffer[4096];
    ssize_t bytes;

    int logs = open(argv[1], O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (logs < 0) {
        const char error[] = "Error: Failed to open log file\n";
        write(STDERR_FILENO, error, sizeof(error));
        exit(EXIT_FAILURE);
    }

    while (bytes = read(STDIN_FILENO, buffer, sizeof(buffer))) {
        if (bytes < 0) {
            const char error[] = "Error: Failed to read stdin\n";
            write(STDERR_FILENO, error, sizeof(error));
            exit(EXIT_FAILURE);
        }

        char result[4096];
        int end_index = 0;
        for (int i = 0; i < bytes; i++) {
            if (!(end_index != 0 && result[end_index - 1] == ' ' && buffer[i] == ' ')) {
                result[end_index] = buffer[i];
                end_index++;
            }
        }

        char log_buffer[4250];
        int log_length = snprintf(log_buffer, sizeof(log_buffer), "(Child 2): My PID is %d. Processing: %s", pid, result);

        int bytes_written = write(logs, log_buffer, log_length);
        if (bytes_written != log_length) {
            const char error[] = "Error: Failed to write to log file\n";
            write(STDERR_FILENO, error, sizeof(error) - 1);
            exit(EXIT_FAILURE);
        }

        bytes_written = write(STDOUT_FILENO, result, end_index);
        if (bytes_written != end_index) {
            const char error[] = "Error: Failed to forward data to next process\n";
            write(STDERR_FILENO, error, sizeof(error) - 1);
            exit(EXIT_FAILURE);
        }
    }

    close(logs);
    return 0;
}