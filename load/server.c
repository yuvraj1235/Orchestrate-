#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>

#define SERVER_PORT 8080
#define WORKER_PORT 9090
#define BUFFER_SIZE 1024

// Protocol Commands
#define CMD_GET_LOAD 1
#define CMD_PROCEED 2
#define CMD_ABORT 0

// Your pool of Worker machines in the lab
char *worker_pool[] = {"127.0.0.1", "192.168.1.101"}; // Update these IPs!
int num_workers = 2;

pthread_mutex_t worker_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *socket_desc) {
    int client_sock = *(int*)socket_desc;
    free(socket_desc);
    char buffer[BUFFER_SIZE];
    long thread_id = (long)pthread_self();

    printf("[Thread %ld] New client connected. Receiving task...\n", thread_id);

    // 1. Receive binary file from Client
    char filename[50];
    sprintf(filename, "task_%ld.bin", thread_id);
    
    long file_size = 0;
    read(client_sock, &file_size, sizeof(file_size));

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    long total_received = 0;
    while (total_received < file_size) {
        int bytes = read(client_sock, buffer, BUFFER_SIZE);
        write(fd, buffer, bytes);
        total_received += bytes;
    }
    close(fd);

    // 2. THE LOAD BALANCER LOGIC
    int worker_sock = -1;
    char *selected_worker_ip = NULL;

    // Lock mutex so multiple threads don't bombard workers simultaneously
    pthread_mutex_lock(&worker_mutex);
    
    for (int i = 0; i < num_workers; i++) {
        int temp_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in worker_addr;
        worker_addr.sin_family = AF_INET;
        worker_addr.sin_port = htons(WORKER_PORT);
        inet_pton(AF_INET, worker_pool[i], &worker_addr.sin_addr);

        if (connect(temp_sock, (struct sockaddr *)&worker_addr, sizeof(worker_addr)) == 0) {
            // Handshake Phase 1: Ask for Load
            int cmd = CMD_GET_LOAD;
            send(temp_sock, &cmd, sizeof(cmd), 0);
            
            double load = 0.0;
            read(temp_sock, &load, sizeof(load));
            
            printf("[Thread %ld] Checked Worker %s - Load: %.2f%%\n", thread_id, worker_pool[i], load);

            if (load < 50.0) {
                // Handshake Phase 2: Accept
                cmd = CMD_PROCEED;
                send(temp_sock, &cmd, sizeof(cmd), 0);
                worker_sock = temp_sock;
                selected_worker_ip = worker_pool[i];
                break; // Found our worker, exit loop!
            } else {
                // Handshake Phase 2: Reject
                cmd = CMD_ABORT;
                send(temp_sock, &cmd, sizeof(cmd), 0);
                close(temp_sock);
            }
        }
    }
    pthread_mutex_unlock(&worker_mutex);

    // 3. Handle Balancer Outcome
    if (worker_sock == -1) {
        printf("[Thread %ld] ERROR: All workers are >50%% load or offline.\n", thread_id);
        char *err = "Server Error: All workers are currently overloaded (>50%). Try again later.\n";
        send(client_sock, err, strlen(err), 0);
    } else {
        printf("[Thread %ld] ASSIGNED to Worker: %s\n", thread_id, selected_worker_ip);
        
        // 4. Send file to selected Worker
        send(worker_sock, &file_size, sizeof(file_size), 0);
        fd = open(filename, O_RDONLY);
        int bytes_read;
        while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
            send(worker_sock, buffer, bytes_read, 0);
        }
        close(fd);

        // 5. Receive output from Worker and pipe to Client
        while ((bytes_read = read(worker_sock, buffer, BUFFER_SIZE)) > 0) {
            send(client_sock, buffer, bytes_read, 0);
        }
        close(worker_sock);
        printf("[Thread %ld] Task complete.\n", thread_id);
    }

    // Cleanup
    close(client_sock);
    unlink(filename); 
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 10); 

    printf("Load Balancer & Dispatcher running. Waiting for clients...\n");

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        int *new_sock_ptr = malloc(sizeof(int));
        *new_sock_ptr = new_socket;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void*)new_sock_ptr);
        pthread_detach(thread_id); 
    }
    return 0;
}