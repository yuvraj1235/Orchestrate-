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
#include <errno.h>

#define UDP_PORT 9091
#define TCP_PORT 9090
#define BUFFER_SIZE 1024

double get_system_load() {
    double load[1];
    if (getloadavg(load, 1) != -1) {
        return (load[0] / sysconf(_SC_NPROCESSORS_ONLN)) * 100.0;
    }
    return 0.0;
}

void *broadcast_load(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket failed");
        return NULL;
    }

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (1) {
        double current_load = get_system_load();
        sendto(sock, &current_load, sizeof(current_load), 0,
               (struct sockaddr *)&addr, sizeof(addr));
        sleep(2);
    }
    return NULL;
}

// Helper: read exactly N bytes
int read_full(int fd, void *buf, size_t size) {
    size_t total = 0;
    while (total < size) {
        int n = read(fd, (char*)buf + total, size - total);
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

void *worker_server(void *arg) {
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv_fd < 0) {
        perror("TCP socket failed");
        return NULL;
    }

    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return NULL;
    }

    if (listen(srv_fd, 5) < 0) {
        perror("Listen failed");
        return NULL;
    }

    printf("Worker active on port %d...\n", TCP_PORT);

    while (1) {
        int cli = accept(srv_fd, NULL, NULL);
        if (cli < 0) {
            perror("Accept failed");
            continue;
        }

        int64_t f_size;

        // FIX: safe read
        if (read_full(cli, &f_size, sizeof(f_size)) < 0) {
            close(cli);
            continue;
        }

        // Unique filenames (avoid collision)
        char src_file[64], bin_file[64];
        sprintf(src_file, "task_%d.c", getpid());
        sprintf(bin_file, "task_%d", getpid());

        int fd = open(src_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            perror("File open failed");
            close(cli);
            continue;
        }

        char buffer[BUFFER_SIZE];
        int64_t total_received = 0;

        while (total_received < f_size) {
            int b = read(cli, buffer, BUFFER_SIZE);
            if (b <= 0) break;
            write(fd, buffer, b);
            total_received += b;
        }

        close(fd);

        // Compile
        char compile_cmd[128];
        sprintf(compile_cmd, "gcc %s -o %s 2> compile_errors.log", src_file, bin_file);
        system(compile_cmd);

        // Execute
        char exec_cmd[128];
        sprintf(exec_cmd, "./%s 2>&1", bin_file);

        FILE *fp = popen(exec_cmd, "r");

        if (fp == NULL) {
            char *err = "Execution failed\n";
            send(cli, err, strlen(err), 0);
        } else {
            while (fgets(buffer, BUFFER_SIZE, fp)) {
                send(cli, buffer, strlen(buffer), 0);
            }
            pclose(fp);
        }

        close(cli);

        unlink(src_file);
        unlink(bin_file);
    }
}

int main() {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, broadcast_load, NULL);
    pthread_create(&t2, NULL, worker_server, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}