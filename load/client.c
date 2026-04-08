#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <source_file.c> <dispatcher_ip>\n", argv[0]);
        return -1;
    }

    // 1. Compile the source code locally into a binary
    printf("Compiling %s locally...\n", argv[1]);
    char compile_cmd[256];
    snprintf(compile_cmd, sizeof(compile_cmd), "gcc %s -o client_payload_bin", argv[1]);
    
    if (system(compile_cmd) != 0) {
        printf("Error: Local compilation failed.\n");
        return -1;
    }

    // 2. Connect to the Dispatcher Server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, argv[2], &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed. Is the Dispatcher running at %s?\n", argv[2]);
        unlink("client_payload_bin");
        return -1;
    }

    // 3. Get binary file size and send it
    struct stat st;
    stat("client_payload_bin", &st);
    long file_size = st.st_size;
    send(sock, &file_size, sizeof(file_size), 0);

    // 4. Send the binary file chunk by chunk
    int fd = open("client_payload_bin", O_RDONLY);
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        send(sock, buffer, bytes_read, 0);
    }
    close(fd);
    printf("Task submitted. Waiting for Dispatcher to return results...\n\n");

    // 5. Receive the final output
    printf("--- Output from Cluster ---\n");
    int valread;
    while ((valread = read(sock, buffer, BUFFER_SIZE)) > 0) {
        write(STDOUT_FILENO, buffer, valread);
    }
    printf("\n---------------------------\n");

    // 6. Cleanup local binary
    unlink("client_payload_bin");
    close(sock);
    return 0;
}