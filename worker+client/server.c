//./server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>

#define UDP_PORT 9091
#define TCP_PORT 9090
#define BUFFER_SIZE 1024
#define MAX_PEERS 10

typedef struct {
    char ip[INET_ADDRSTRLEN];
    double load;
    long last_seen;
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

// --- 1. HEARTBEAT: Broadcasts "I am here and my load is X" ---
void *broadcast_load(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (1) {
        double current_load = get_system_load();
        sendto(sock, &current_load, sizeof(current_load), 0, (struct sockaddr *)&addr, sizeof(addr));
        sleep(2); // Broadcast every 2 seconds
    }
    return NULL;
}

// --- 2. DISCOVERY: Listens for heartbeats from other nodes ---
void *listen_for_peers(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));

    while (1) {
        double received_load;
        struct sockaddr_in peer_addr;
        socklen_t addr_len = sizeof(peer_addr);
        recvfrom(sock, &received_load, sizeof(received_load), 0, (struct sockaddr *)&peer_addr, &addr_len);

        char *ip = inet_ntoa(peer_addr.sin_addr);
        
        pthread_mutex_lock(&table_mutex);
        int found = 0;
        for (int i = 0; i < peer_count; i++) {
            if (strcmp(routing_table[i].ip, ip) == 0) {
                routing_table[i].load = received_load;
                found = 1;
                break;
            }
        }
        if (!found && peer_count < MAX_PEERS) {
            strcpy(routing_table[peer_count].ip, ip);
            routing_table[peer_count].load = received_load;
            peer_count++;
        }
        pthread_mutex_unlock(&table_mutex);
    }
}

// --- 3. TCP WORKER: Executes received tasks ---
void *worker_server(void *arg) {
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(TCP_PORT), INADDR_ANY};
    bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv_fd, 5);

    char buffer[BUFFER_SIZE];
    while (1) {
        int cli = accept(srv_fd, NULL, NULL);
        long f_size;
        read(cli, &f_size, sizeof(f_size));

        int fd = open("task_exec", O_WRONLY | O_CREAT | O_TRUNC, 0777);
        long rec = 0;
        while (rec < f_size) {
            int b = read(cli, buffer, BUFFER_SIZE);
            write(fd, buffer, b);
            rec += b;
        }
        close(fd);

        // Execute and send output back
        FILE *fp = popen("./task_exec", "r");
        while (fgets(buffer, BUFFER_SIZE, fp)) {
            send(cli, buffer, strlen(buffer), 0);
        }
        pclose(fp);
        close(cli);
        unlink("task_exec");
    }
}

// --- 4. CLIENT: Finds best node and sends task ---
void run_client(char *source_file) {
    pthread_mutex_lock(&table_mutex);
    if (peer_count == 0) {
        printf("No peers discovered yet. Wait a few seconds...\n");
        pthread_mutex_unlock(&table_mutex);
        return;
    }

    int best_idx = 0;
    for (int i = 1; i < peer_count; i++) {
        if (routing_table[i].load < routing_table[best_idx].load) best_idx = i;
    }
    char target_ip[INET_ADDRSTRLEN];
    strcpy(target_ip, routing_table[best_idx].ip);
    pthread_mutex_unlock(&table_mutex);

    printf("Best Node: %s (Load: %.2f%%). Sending task...\n", target_ip, routing_table[best_idx].load);

    // Standard TCP send logic...
    char cmd[256];
    sprintf(cmd, "gcc %s -o task_bin", source_file);
    system(cmd);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, target_ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
        struct stat st; stat("task_bin", &st);
        long size = st.st_size;
        send(sock, &size, sizeof(size), 0);
        int fd = open("task_bin", O_RDONLY);
        char buf[BUFFER_SIZE];
        int b;
        while ((b = read(fd, buf, BUFFER_SIZE)) > 0) send(sock, buf, b, 0);
        close(fd);
        while ((b = read(sock, buf, BUFFER_SIZE)) > 0) write(1, buf, b);
        close(sock);
    }
    unlink("task_bin");
}

int main(int argc, char *argv[]) {
    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, broadcast_load, NULL);
    pthread_create(&t2, NULL, listen_for_peers, NULL);
    pthread_create(&t3, NULL, worker_server, NULL);

    if (argc == 2) {
        sleep(3); // Wait to discover peers
        run_client(argv[1]);
    } else {
        printf("Running in Background Mode. Use: ./node <file.c> to submit tasks.\n");
        pthread_join(t1, NULL); // Keep alive
    }
    return 0;
}