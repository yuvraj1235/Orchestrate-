#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdint.h>


#define UDP_BROADCAST_PORT 9091
#define BUFFER_SIZE 1024

int MY_TCP_PORT;

// Structure to match the Node.js buffer reading
typedef struct {
    int32_t tcp_port;
    double load;
} HeartbeatMsg;

double get_system_load() {
    double load[1];
    if (getloadavg(load, 1) != -1) {
        return (load[0] * 10.0); // Scale for visibility
    }
    return 0.0;
}

// 1. BROADCAST: Tells the Web UI "I am here on port X with load Y"
void *broadcast_load(void *arg) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_BROADCAST_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    while (1) {
        HeartbeatMsg msg;
        msg.tcp_port = (int32_t)MY_TCP_PORT;
        msg.load = get_system_load();

        sendto(sock, &msg, sizeof(msg), 0, (struct sockaddr *)&addr, sizeof(addr));
        sleep(2);
    }
    return NULL;
}

// 2. TCP WORKER: Receives, compiles, and runs the .c file
void *worker_server(void *arg) {
    int srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {AF_INET, htons(MY_TCP_PORT), INADDR_ANY};
    if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed. Try a different port.");
        exit(1);
    }
    listen(srv_fd, 5);

    char buffer[BUFFER_SIZE];
    while (1) {
        int cli = accept(srv_fd, NULL, NULL);
        int64_t f_size = 0;
        read(cli, &f_size, sizeof(f_size));

        char src_name[64], bin_name[64];
        sprintf(src_name, "incoming_%d.c", MY_TCP_PORT);
        sprintf(bin_name, "bin_%d", MY_TCP_PORT);

        int fd = open(src_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        int64_t received = 0;
        while (received < f_size) {
            int b = read(cli, buffer, BUFFER_SIZE);
            if (b <= 0) break;
            write(fd, buffer, b);
            received += b;
        }
        close(fd);

        // Compile and Execute
        char cmd[256];
        sprintf(cmd, "gcc %s -o %s && ./%s 2>&1", src_name, bin_name, bin_name);
        
        FILE *fp = popen(cmd, "r");
        if (fp) {
            while (fgets(buffer, BUFFER_SIZE, fp)) {
                send(cli, buffer, strlen(buffer), 0);
            }
            pclose(fp);
        }

        close(cli);
        unlink(src_name);
        unlink(bin_name);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Error: Provide a unique port (e.g., ./node 9090)\n");
        return 1;
    }
    MY_TCP_PORT = atoi(argv[1]);

    pthread_t t1, t2;
    pthread_create(&t1, NULL, broadcast_load, NULL);
    pthread_create(&t2, NULL, worker_server, NULL);

    printf("Node running on Localhost:%d. Check Web UI for status.\n", MY_TCP_PORT);
    pthread_join(t1, NULL);
    return 0;
}