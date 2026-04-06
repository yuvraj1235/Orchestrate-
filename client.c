#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9000
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    // 1. Check if the user provided the file and IP!
    if (argc != 3) {
        printf("Usage: %s <source_file.c> <server_ip>\n", argv[0]);
        return -1;
    }

    char *source_file = argv[1];
    char *ip = argv[2];

    // 2. Compile the specific file the user asked for
    char compile_cmd[256];
    sprintf(compile_cmd, "gcc %s -o program.out", source_file);
    if (system(compile_cmd) != 0) {
        printf("Compilation failed locally.\n");
        return -1;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // 3. Use the IP from the command line
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed\n");
        return -1;
    }

    // Send executable
    FILE *fp = fopen("program.out", "rb");
    int bytes;

    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(sock, buffer, bytes, 0);
    }
    fclose(fp);

    shutdown(sock, SHUT_WR);

    printf("---- Output from Remote Machine ----\n");

    // Receive output
    while ((bytes = recv(sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }

    close(sock);
    return 0;
}