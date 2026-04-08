#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#define WORKER_PORT 9090
#define BUFFER_SIZE 1024

// Protocol Commands
#define CMD_GET_LOAD 1
#define CMD_PROCEED 2
#define CMD_ABORT 0

// --- LINUX NATIVE CPU LOAD CALCULATION ---
double get_linux_cpu_load() {
    double load[3];
    // getloadavg reads from /proc/loadavg (1-min, 5-min, 15-min averages)
    if (getloadavg(load, 3) != -1) {
        // Find how many CPU cores the machine has
        int cores = sysconf(_SC_NPROCESSORS_ONLN);
        // Calculate percentage for the 1-minute load average
        return (load[0] / cores) * 100.0;
    }
    return 0.0; // Fallback
}

int main() {
    int worker_fd, dispatcher_sock;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];

    worker_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(worker_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(WORKER_PORT);

    bind(worker_fd, (struct sockaddr *)&address, sizeof(address));
    listen(worker_fd, 5);

    printf("Worker Node running. Listening for tasks on port %d...\n", WORKER_PORT);

    while (1) {
        dispatcher_sock = accept(worker_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        int cmd;
        read(dispatcher_sock, &cmd, sizeof(cmd));

        if (cmd == CMD_GET_LOAD) {
            double current_load = get_linux_cpu_load();
            
            // NOTE: For demonstration purposes in the lab, you might want to uncomment 
            // the next line to fake a random load so you can actually show the balancer working!
            // current_load = rand() % 100; 

            printf("Dispatcher requested load. My CPU load is: %.2f%%\n", current_load);
            send(dispatcher_sock, &current_load, sizeof(current_load), 0);

            // Wait for Dispatcher's decision
            read(dispatcher_sock, &cmd, sizeof(cmd));

            if (cmd == CMD_ABORT) {
                printf("    -> Load too high (or aborted). Connection closed.\n\n");
                close(dispatcher_sock);
                continue; // Wait for next connection
            }
            
            if (cmd == CMD_PROCEED) {
                printf("    -> Task assigned! Receiving binary...\n");
                // Receive the binary
                long file_size = 0;
                read(dispatcher_sock, &file_size, sizeof(file_size));

                int fd = open("received_task_bin", O_WRONLY | O_CREAT | O_TRUNC, 0777); 
                long total_received = 0;
                while (total_received < file_size) {
                    int bytes = read(dispatcher_sock, buffer, BUFFER_SIZE);
                    write(fd, buffer, bytes);
                    total_received += bytes;
                }
                close(fd);

                // Execute the binary and capture the output
                int pipefd[2];
                pipe(pipefd);
                pid_t pid = fork();

                if (pid == 0) {
                    dup2(pipefd[1], STDOUT_FILENO);
                    dup2(pipefd[1], STDERR_FILENO);
                    close(pipefd[0]);
                    close(pipefd[1]);
                    execl("./received_task_bin", "./received_task_bin", (char *)NULL);
                    exit(0); 
                } else {
                    close(pipefd[1]);
                    int bytes_read;
                    while ((bytes_read = read(pipefd[0], buffer, BUFFER_SIZE)) > 0) {
                        send(dispatcher_sock, buffer, bytes_read, 0);
                    }
                    close(pipefd[0]);
                    wait(NULL); 
                }

                unlink("received_task_bin");
                close(dispatcher_sock);
                printf("[+] Execution complete. Results sent.\n\n");
            }
        }
    }
    return 0;
}