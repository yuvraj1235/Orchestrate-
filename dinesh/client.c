#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define GREEN "\033[1;32m"
#define RED   "\033[1;31m"
#define RESET "\033[0m"

void compile_and_send(const char* filename, const char* master_ip) {
    char cmd[256];
    
    // 1. Local Compilation Check
    sprintf(cmd, "gcc %s -o program.out 2>/dev/null", filename);
    if (system(cmd) != 0) {
        printf(RED "❌ Local compilation failed. Please check %s\n" RESET, filename);
        return;
    }

    // 2. Network Setup
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in master_addr;
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, master_ip, &master_addr.sin_addr) <= 0) {
        printf(RED "❌ Invalid Master IP address.\n" RESET);
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr*)&master_addr, sizeof(master_addr)) < 0) {
        printf(RED "❌ Could not reach Master Server at %s\n" RESET, master_ip);
        close(sock);
        return;
    }

    printf("📤 Sending to Master Node (%s)...\n", master_ip);

    // 3. Send Executable Data
    FILE *fp = fopen("program.out", "rb");
    if (!fp) {
        printf(RED "❌ Failed to open compiled binary.\n" RESET);
        close(sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send(sock, buffer, bytes, 0);
    }
    fclose(fp);
    
    // Shutdown write-half so Master knows file transfer is complete
    shutdown(sock, SHUT_WR); 

    printf("⚙️  Master is delegating task to minimum load node...\n");
    printf("\n---------- EXECUTION RESULT ----------\n");

    // 4. Receive and Print Result
    while ((bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes] = 0;
        printf("%s", buffer);
    }

    printf(GREEN "\nTask Finished\n" RESET);
    printf("--------------------------------------\n\n");

    close(sock);
}

int main(int argc, char *argv[]) {
    char master_ip[50];
    char filename[100];

    // Get Master IP from argument or user input
    if (argc == 2) {
        strncpy(master_ip, argv[1], 49);
    } else {
        printf("Enter Master Server IP: ");
        scanf("%49s", master_ip);
    }

    printf("\n" GREEN "--- Distributed Client Active ---" RESET "\n");
    printf("Target Master: %s\n", master_ip);
    printf("Type 'exit' to quit the program.\n\n");

    while (1) {
        printf("Submit File (.c)> ");
        scanf("%99s", filename);

        if (strcmp(filename, "exit") == 0) {
            break;
        }

        // Check if local file exists
        if (access(filename, F_OK) == -1) {
            printf(RED "❌ Error: File '%s' not found.\n" RESET, filename);
            continue;
        }

        compile_and_send(filename, master_ip);
    }

    printf("Exiting client. Goodbye!\n");
    return 0;
}