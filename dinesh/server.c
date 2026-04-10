#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Structure to hold server information
typedef struct {
    char ip[16];
    int current_tasks;
    double latency;
    double score;
} RemoteServer;

// Global tracker for local server tasks
int local_tasks = 2;

// Step 1: Discovery (Mocking the UDP Broadcast)
int discover_servers(RemoteServer *list) {
    printf("Broadcasting UDP 'DISCOVER' (255.255.255.255)...\n");
    
    // In a real C app, you would use <sys/socket.h> and <netinet/in.h> here.
    // For now, we manually fill the list with "ALIVE" responses.
    
    strcpy(list[0].ip, "192.168.1.10");
    list[0].current_tasks = 1;
    list[0].latency = 15.0;

    strcpy(list[1].ip, "192.168.1.20");
    list[1].current_tasks = 5;
    list[1].latency = 5.0;

    return 2; // Number of servers found
}

// Step 2: Scoring Logic
RemoteServer select_best_server(RemoteServer *list, int count) {
    int best_index = 0;
    
    for (int i = 0; i < count; i++) {
        // Score = load + (0.1 × latency)
        list[i].score = (double)list[i].current_tasks + (0.1 * list[i].latency);
        printf("Checked Server %s | Score: %.2f\n", list[i].ip, list[i].score);
        
        if (list[i].score < list[best_index].score) {
            best_index = i;
        }
    }
    return list[best_index];
}

// Step 3: Decision Tree and Execution
void handle_task(const char *binary_data) {
    RemoteServer discovered[10];
    int count = discover_servers(discovered);
    
    // Calculate local score
    double local_score = (double)local_tasks + (0.1 * 0.0);
    printf("Local Server Score: %.2f\n", local_score);

    RemoteServer best_remote = select_best_server(discovered, count);

    if (best_remote.score < local_score) {
        // YES -> Forward task
        printf("\n--- DECISION: FORWARD ---\n");
        printf("Sending binary to %s...\n", best_remote.ip);
        printf("[Executed by Server: %s] Returning output to client...\n", best_remote.ip);
    } else {
        // NO -> Execute locally
        printf("\n--- DECISION: EXECUTE LOCALLY ---\n");
        
        local_tasks++; // Increment task counter
        
        // Save binary to temp.out
        FILE *fp = fopen("temp.out", "wb");
        if (fp != NULL) {
            fprintf(fp, "%s", binary_data);
            fclose(fp);
            
            printf("Running: ./temp.out > result.txt\n");
            // system("./temp.out > result.txt"); // Real shell execution
            
            local_tasks--; // Decrement task counter
            
            printf("[Executed by Server: 127.0.0.1]\n");
            printf("Contents of result.txt sent to client.\n");
        }
    }
}

int main() {
    const char *mock_binary = "BINARY_PAYLOAD_DATA";
    handle_task(mock_binary);
    return 0;
}