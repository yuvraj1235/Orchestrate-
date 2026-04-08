#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <semaphore.h>#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define STORAGE_DIR "server_logs"

// ANSI Color Codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

sem_t *lock;

// Helper function to print beautiful logs
void log_status(const char* color, const char* status, const char* message) {
    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0'; 

    printf("%s[%s]%s %s%s%-10s%s %s\n", 
           CYAN, date, RESET, 
           BOLD, color, status, RESET, 
           message);
}

void handle_client(int client_socket, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    int bytes;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    char msg[150];
    sprintf(msg, "Handling request from %s", client_ip);
    log_status(BLUE, "PROCESS", msg);

    // Create unique binary name for this process
    char bin_path[100];
    sprintf(bin_path, "%s/received_%d.out", STORAGE_DIR, getpid());

    FILE *fp = fopen(bin_path, "wb");
    if (!fp) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }

    while ((bytes = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
    }
    fclose(fp);

    // Critical section (Execution)
    log_status(YELLOW, "LOCK", "Waiting for semaphore...");
    sem_wait(lock);
    
    log_status(YELLOW, "EXEC", "Running binary...");

    // Make executable
    char cmd[300];
    sprintf(cmd, "chmod +x %s", bin_path);
    system(cmd);

    // Define output path (Saved permanently)
    char out_path[100];
    sprintf(out_path, "%s/output_%ld_pid%d.txt", STORAGE_DIR, time(NULL), getpid());

    // Execute and redirect output + errors to the server file
    sprintf(cmd, "./%s > %s 2>&1", bin_path, out_path);
    system(cmd);

    sem_post(lock);
    
    // Send the saved output back to the client
    fp = fopen(out_path, "r");
    if (fp) {
        while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
            send(client_socket, buffer, bytes, 0);
        }
        fclose(fp);
    }

    // Cleanup the binary, but DO NOT unlink out_path (saves it on system)
    unlink(bin_path);
    
    sprintf(msg, "Log saved to: %s", out_path);
    log_status(GREEN, "DONE", msg);

    close(client_socket);
    exit(0);
}

int main() {
    // Prevent zombie processes
    signal(SIGCHLD, SIG_IGN);

    // Ensure storage directory exists
    struct stat st = {0};
    if (stat(STORAGE_DIR, &st) == -1) {
        mkdir(STORAGE_DIR, 0700);
    }

    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    sem_unlink("/exec_lock");
    lock = sem_open("/exec_lock", O_CREAT, 0644, 1);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    listen(server_fd, 5);

    printf("\n%s%s============================================%s\n", BOLD, BLUE, RESET);
    printf("%s%s       REMOTE EXECUTION SERVER (C)           %s\n", BOLD, WHITE, RESET);
    printf("%s%s   Port: %d | PID: %d | Logs: %s   %s\n", BOLD, CYAN, PORT, getpid(), STORAGE_DIR, RESET);
    printf("%s%s============================================%s\n\n", BOLD, BLUE, RESET);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        log_status(GREEN, "CONNECT", "New client connected");

        if (fork() == 0) {
            close(server_fd);
            handle_client(client_socket, address);
        }

        close(client_socket);
    }

    sem_close(lock);
    sem_unlink("/exec_lock");
    return 0;
}
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define STORAGE_DIR "server_logs"

// ANSI Color Codes
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

sem_t *lock;

// Helper function to print beautiful logs
void log_status(const char* color, const char* status, const char* message) {
    time_t now;
    time(&now);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0'; 

    printf("%s[%s]%s %s%s%-10s%s %s\n", 
           CYAN, date, RESET, 
           BOLD, color, status, RESET, 
           message);
}

void handle_client(int client_socket, struct sockaddr_in client_addr) {
    char buffer[BUFFER_SIZE];
    int bytes;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    char msg[150];
    sprintf(msg, "Handling request from %s", client_ip);
    log_status(BLUE, "PROCESS", msg);

    // Create unique binary name for this process
    char bin_path[100];
    sprintf(bin_path, "%s/received_%d.out", STORAGE_DIR, getpid());

    FILE *fp = fopen(bin_path, "wb");
    if (!fp) {
        perror("File open failed");
        exit(EXIT_FAILURE);
    }

    while ((bytes = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        fwrite(buffer, 1, bytes, fp);
    }
    fclose(fp);

    // Critical section (Execution)
    log_status(YELLOW, "LOCK", "Waiting for semaphore...");
    sem_wait(lock);
    
    log_status(YELLOW, "EXEC", "Running binary...");

    // Make executable
    char cmd[300];
    sprintf(cmd, "chmod +x %s", bin_path);
    system(cmd);

    // Define output path (Saved permanently)
    char out_path[100];
    sprintf(out_path, "%s/output_%ld_pid%d.txt", STORAGE_DIR, time(NULL), getpid());

    // Execute and redirect output + errors to the server file
    sprintf(cmd, "./%s > %s 2>&1", bin_path, out_path);
    system(cmd);

    sem_post(lock);
    
    // Send the saved output back to the client
    fp = fopen(out_path, "r");
    if (fp) {
        while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
            send(client_socket, buffer, bytes, 0);
        }
        fclose(fp);
    }

    // Cleanup the binary, but DO NOT unlink out_path (saves it on system)
    unlink(bin_path);
    
    sprintf(msg, "Log saved to: %s", out_path);
    log_status(GREEN, "DONE", msg);

    close(client_socket);
    exit(0);
}

int main() {
    // Prevent zombie processes
    signal(SIGCHLD, SIG_IGN);

    // Ensure storage directory exists
    struct stat st = {0};
    if (stat(STORAGE_DIR, &st) == -1) {
        mkdir(STORAGE_DIR, 0700);
    }

    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    sem_unlink("/exec_lock");
    lock = sem_open("/exec_lock", O_CREAT, 0644, 1);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    listen(server_fd, 5);

    printf("\n%s%s============================================%s\n", BOLD, BLUE, RESET);
    printf("%s%s       REMOTE EXECUTION SERVER (C)           %s\n", BOLD, WHITE, RESET);
    printf("%s%s   Port: %d | PID: %d | Logs: %s   %s\n", BOLD, CYAN, PORT, getpid(), STORAGE_DIR, RESET);
    printf("%s%s============================================%s\n\n", BOLD, BLUE, RESET);

    while (1) {
        client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        
        log_status(GREEN, "CONNECT", "New client connected");

        if (fork() == 0) {
            close(server_fd);
            handle_client(client_socket, address);
        }

        close(client_socket);
    }

    sem_close(lock);
    sem_unlink("/exec_lock");
    return 0;
}