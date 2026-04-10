#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

#define UDP_PORT 9000
#define MAX_PEERS 20
#define BUFFER_SIZE 1024


typedef struct {
    char ip[16];
    int port;
    int load;
    time_t last_seen;
} Peer;

Peer peer_table[MAX_PEERS];
int peer_count = 0;
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

int my_port;
int my_load;

void* broadcast_load(void* arg) {
    int sock;
    struct sockaddr_in addr;
    int broadcast = 1;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (1) {
        char msg[128];
        my_load = rand() % 101;
        sprintf(msg, "%d:%d", my_port, my_load);
        sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr));
        sleep(2);
    }
    return NULL;
}

void* listen_discovery(void* arg) {
    int sock;
    struct sockaddr_in addr, peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    while (1) {
        int len = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&peer_addr, &addr_len);
        buffer[len] = '\0';

        char* sender_ip = inet_ntoa(peer_addr.sin_addr);
        int p_port, p_load;
        sscanf(buffer, "%d:%d", &p_port, &p_load);

        pthread_mutex_lock(&table_mutex);
        int found = 0;
        for (int i = 0; i < peer_count; i++) {
            if (peer_table[i].port == p_port && strcmp(peer_table[i].ip, sender_ip) == 0) {
                peer_table[i].load = p_load;
                peer_table[i].last_seen = time(NULL);
                found = 1;
                break;
            }
        }
        if (!found && peer_count < MAX_PEERS) {
            strcpy(peer_table[peer_count].ip, sender_ip);
            peer_table[peer_count].port = p_port;
            peer_table[peer_count].load = p_load;
            peer_table[peer_count].last_seen = time(NULL);
            peer_count++;
        }
        pthread_mutex_unlock(&table_mutex);
    }
    return NULL;
}

void* tcp_server(void* arg) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(my_port);

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        int valread = read(new_socket, buffer, BUFFER_SIZE);
        buffer[valread] = '\0';
        printf("\n[TASK FROM %s]: %s\n> ", inet_ntoa(address.sin_addr), buffer);
        send(new_socket, "ACK", 3, 0);
        close(new_socket);
    }
    return NULL;
}

void send_task(char* task) {
    int target_port = -1;
    char target_ip[16];
    int min_load = 101;

    pthread_mutex_lock(&table_mutex);
    for (int i = 0; i < peer_count; i++) {
        if (peer_table[i].load < min_load) {
            min_load = peer_table[i].load;
            target_port = peer_table[i].port;
            strcpy(target_ip, peer_table[i].ip);
        }
    }
    pthread_mutex_unlock(&table_mutex);

    if (target_port == -1) {
        printf("Searching for peers...\n");
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(target_port);
    inet_pton(AF_INET, target_ip, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == 0) {
        send(sock, task, strlen(task), 0);
        char rx[10] = {0};
        read(sock, rx, 10);
        printf("Sent to %s:%d (Load: %d). Resp: %s\n", target_ip, target_port, min_load, rx);
    }
    close(sock);
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }
    my_port = atoi(argv[1]);
    srand(time(NULL) + my_port);

    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, broadcast_load, NULL);
    pthread_create(&t2, NULL, listen_discovery, NULL);
    pthread_create(&t3, NULL, tcp_server, NULL);

    char input[BUFFER_SIZE];
    while (1) {
        printf("> Task: ");
        fgets(input, BUFFER_SIZE, stdin);
        input[strcspn(input, "\n")] = 0;
        send_task(input);
    }
    return 0;
}