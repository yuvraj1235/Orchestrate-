// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>

#define PORT 9000
#define BUFFER_SIZE 1024

sem_t *lock;

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes;

    // Unique file per process (avoid overwrite)
    char filename[50];
    sprintf(filename, "received_%d.out", getpid());

    FILE *fp = fopen(filename, "wb");

    while ((bytes = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
    }
    fclose(fp);

    // Critical section (execution)
    sem_wait(lock);

    char cmd[200];
    sprintf(cmd, "chmod +x %s", filename);
    system(cmd);

    char outfile[50];
    sprintf(outfile, "output_%d.txt", getpid());

    sprintf(cmd, "./%s > %s", filename, outfile);
    system(cmd);

    sem_post(lock);

    // Send output back
    fp = fopen(outfile, "r");
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(client_socket, buffer, bytes, 0);
    }
    fclose(fp);

    close(client_socket);
    exit(0);
}

int main() {
    signal(SIGCHLD, SIG_IGN);
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Named semaphore
    lock = sem_open("/exec_lock", O_CREAT, 0644, 1);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);

    printf("Server running on port %d...\n", PORT);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);

        if (fork() == 0) {
            close(server_fd);
            handle_client(client_socket);
        }

        close(client_socket);
    }

    sem_close(lock);
    sem_unlink("/exec_lock");

    return 0;
}