#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9000
#define BUFFER_SIZE 1024

#define GREEN "\033[1;32m"
#define RESET "\033[0m"

int main(int argc, char *argv[]) {

    if (argc != 3) {
        printf("Usage: %s <file.c> <server_ip>\n", argv[0]);
        return -1;
    }

    char cmd[200];
    sprintf(cmd, "gcc %s -o program.out", argv[1]);

    if (system(cmd) != 0) {
        printf("Compilation failed\n");
        return -1;
    }

    printf("\n====================================\n");
    printf("   DISTRIBUTED EXECUTION SYSTEM\n");
    printf("====================================\n");

    printf("📤 Sending executable...\n");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, argv[2], &addr.sin_addr);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    FILE *fp = fopen("program.out", "rb");
    char buffer[BUFFER_SIZE];
    int bytes;

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
        send(sock, buffer, bytes, 0);

    fclose(fp);
    shutdown(sock, SHUT_WR);

    printf("⚙️  Processing...\n");

    printf("\n========== RESULT ==========\n");

    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes] = 0;
        printf("%s", buffer);
    }

    printf(GREEN "\nExecution Completed Successfully\n" RESET);

    close(sock);
}