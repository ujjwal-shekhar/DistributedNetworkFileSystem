#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main() {
    int client_socket;
    struct sockaddr_in server_addr;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080); // Use the same port as the server
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Use the server's IP address

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr) == -1) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char message[] = "Hello from the client\n";
    send(client_socket, message, strlen(message), 0);

    char buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        perror("Server disconnected");
    } else {
        buffer[bytes_received] = '\0';
        printf("Received from server: %s", buffer);
    }

    close(client_socket);

    return 0;
}
