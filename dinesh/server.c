#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define TCP_PORT 9000
#define UDP_PORT 9001
#define BUFFER_SIZE 1024
#define MAX_SERVERS 20

char servers[MAX_SERVERS][20];
int server_count = 0;

int current_tasks = 0;

/* ================= GET OWN IP ================= */
char* get_my_ip() {
    static char ip[20];
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);

    connect(sock, (struct sockaddr*)&serv, sizeof(serv));

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    getsockname(sock, (struct sockaddr*)&name, &namelen);

    strcpy(ip, inet_ntoa(name.sin_addr));
    close(sock);

    return ip;
}

/* ================= DISCOVERY ================= */
void discover_servers() {

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = inet_addr("255.255.255.255");

    sendto(sock, "DISCOVER", 8, 0, (struct sockaddr*)&addr, sizeof(addr));

    struct sockaddr_in sender;
    socklen_t len = sizeof(sender);
    char buffer[BUFFER_SIZE];

    struct timeval tv = {1, 0};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    server_count = 0;

    while (recvfrom(sock, buffer, BUFFER_SIZE, 0,
           (struct sockaddr*)&sender, &len) > 0) {

        char *ip = inet_ntoa(sender.sin_addr);

        if (strcmp(ip, get_my_ip()) != 0) {
            strcpy(servers[server_count++], ip);
        }
    }

    close(sock);
}

/* ================= UDP LISTENER ================= */
void udp_listener() {

    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t len = sizeof(sender);

    while (1) {
        int n = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                        (struct sockaddr*)&sender, &len);

        if (strncmp(buffer, "DISCOVER", 8) == 0) {
            sendto(sock, "ALIVE", 5, 0,
                   (struct sockaddr*)&sender, len);
        }
    }
}

/* ================= LOAD ================= */
int get_load(char *ip) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        return 9999;

    send(sock, "LOAD", 4, 0);

    char buf[10];
    int n = recv(sock, buf, 10, 0);
    buf[n] = 0;

    close(sock);
    return atoi(buf);
}

/* ================= LATENCY ================= */
double measure_latency(char *ip) {

    struct timeval start, end;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    gettimeofday(&start, NULL);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0)
        return 9999;

    gettimeofday(&end, NULL);

    close(sock);

    return (end.tv_sec - start.tv_sec) * 1000.0 +
           (end.tv_usec - start.tv_usec) / 1000.0;
}

/* ================= FORWARD ================= */
void forward_task(char *ip, char *buffer, int size, int client) {

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    send(sock, buffer, size, 0);
    shutdown(sock, SHUT_WR);

    char temp[BUFFER_SIZE];
    int n;

    while ((n = recv(sock, temp, BUFFER_SIZE, 0)) > 0)
        send(client, temp, n, 0);

    close(sock);
}

/* ================= MAIN ================= */
int main() {

    if (fork() == 0) {
        udp_listener();
        exit(0);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Server running on %s\n", get_my_ip());

    while (1) {

        int client = accept(server_fd, NULL, NULL);

        if (fork() == 0) {

            char buffer[BUFFER_SIZE];
            int bytes = recv(client, buffer, BUFFER_SIZE, 0);

            /* LOAD REQUEST */
            if (strncmp(buffer, "LOAD", 4) == 0) {
                char reply[10];
                sprintf(reply, "%d", current_tasks);
                send(client, reply, strlen(reply), 0);
                close(client);
                exit(0);
            }

            /* DISCOVER SERVERS */
            discover_servers();

            /* SELECT BEST */
            double best_score = current_tasks;
            int best = -1;

            for (int i = 0; i < server_count; i++) {

                int load = get_load(servers[i]);
                double latency = measure_latency(servers[i]);

                double score = load + 0.1 * latency;

                if (score < best_score) {
                    best_score = score;
                    best = i;
                }
            }

            /* FORWARD */
            if (best != -1) {
                forward_task(servers[best], buffer, bytes, client);
                close(client);
                exit(0);
            }

            /* EXECUTE LOCALLY */
            FILE *fp = fopen("temp.out", "wb");
            fwrite(buffer, 1, bytes, fp);

            while ((bytes = recv(client, buffer, BUFFER_SIZE, 0)) > 0)
                fwrite(buffer, 1, bytes, fp);

            fclose(fp);

            current_tasks++;

            system("chmod +x temp.out");
            system("./temp.out > result.txt");

            current_tasks--;

            /* SEND SERVER INFO */
            char header[100];
            sprintf(header, "\n[Executed by Server: %s]\n", get_my_ip());
            send(client, header, strlen(header), 0);

            fp = fopen("result.txt", "r");
            while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
                send(client, buffer, bytes, 0);

            fclose(fp);

            close(client);
            exit(0);
        }

        close(client);
    }
}