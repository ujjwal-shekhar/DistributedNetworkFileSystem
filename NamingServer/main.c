#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

void* client_handler(void* arg) {
    int client_socket = *(int*)arg;

    // Implement your client handling logic here

    char buffer[1024];
    ssize_t bytes_received;
    
    while (1) {
        // Receive data from the client
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // Client disconnected or an error occurred
            break;
        }

        // Process and respond to the received data (modify as needed)
        // For example, you can print the received data
        buffer[bytes_received] = '\0';
        printf("Received: %s", buffer);

        // Send a response (modify as needed)
        const char* response = "Server: Message received\n";
        send(client_socket, response, strlen(response), 0);
    }

    // Close the client socket
    close(client_socket);

    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080); // Use an available port

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr) == -1)) {
        perror("Socket binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Client connection failed");
            continue;
        }

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_handler, &client_socket) != 0) {
            perror("Thread creation failed");
            close(client_socket);
        }
    }

    close(server_socket);

    return 0;
}
