#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdint.h>

using namespace std;

#define UDP_PORT 9091
#define TCP_PORT 9090
#define BUFFER_SIZE 1024
#define MAX_PEERS 10

typedef struct {
    char ip[INET_ADDRSTRLEN];
    double load;
} Peer;

Peer routing_table[MAX_PEERS];
int peer_count = 0;
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

double get_system_load() {
    double load[1];
    if (getloadavg(load, 1) != -1) {
        return (load[0] / sysconf(_SC_NPROCESSORS_ONLN)) * 100.0;
    }
    return 0.0;
}

void *broadcast_load(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (1) {
        double current_load = get_system_load();
        sendto(sock, &current_load, sizeof(current_load), 0, (struct sockaddr *)&addr, sizeof(addr));
        sleep(2);
    }
    return NULL;
}

void *worker_server(void *arg) {
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {AF_INET, htons(TCP_PORT), INADDR_ANY};
    bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv_fd, 5);

    char buffer[BUFFER_SIZE];
    while (1) {
        int cli = accept(srv_fd, NULL, NULL);
        int64_t f_size = 0;
        
        // Read 8-byte file size
        read(cli, &f_size, sizeof(f_size));

        int fd = open("incoming_task.c", O_WRONLY | O_CREAT | O_TRUNC, 0666);
        int64_t total_received = 0;
        while (total_received < f_size) {
            int b = read(cli, buffer, BUFFER_SIZE);
            if (b <= 0) break;
            write(fd, buffer, b);
            total_received += b;
        }
        close(fd);

        // Compile and Execute
        system("gcc incoming_task.c -o task_bin 2> compile_errors.log");
        FILE *fp = popen("./task_bin 2>&1", "r");
        
        if (fp == NULL) {
            const char *err = "Execution failed.\n";
            send(cli, err, strlen(err), 0);
        } else {
            while (fgets(buffer, BUFFER_SIZE, fp)) {
                send(cli, buffer, strlen(buffer), 0);
            }
            pclose(fp);
        }
        
        close(cli);
        unlink("incoming_task.c");
        unlink("task_bin");
    }
}

int main() {
    pthread_t t1, t3;
    pthread_create(&t1, NULL, broadcast_load, NULL);
    pthread_create(&t3, NULL, worker_server, NULL);

    printf("Worker active. Listening for tasks on TCP %d...\n", TCP_PORT);
    pthread_join(t1, NULL);
    return 0;
}