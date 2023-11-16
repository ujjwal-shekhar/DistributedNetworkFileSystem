// nm.c
#include "../utils/headers.h"

// Make a list of client fds
int client_fds[MAX_CLIENTS];

// List of client threads
pthread_t clientThreads[MAX_CLIENTS];

// List of servers
ServerDetails servers[MAX_SERVERS];

// Function to handle client communication in a separate thread
void* handleClientCommunication(void* arg) {
    int clientSocket = *((int*)arg);

    while (1) {
        // Add your logic to handle communication with the client
        // For example, you can receive requests, process them, and send acknowledgments
        // Receive a struct Client Request
        ClientRequest clientRequest;
        if (recv(clientSocket, &clientRequest, sizeof(clientRequest), 0) < 0) {
            perror("Error receiving client request");
            break;  // Exit the loop if there is an error
        }

        // Print the Request details
        printf("Argument 1: %s\n", clientRequest.arg1);

        // Send a success acknowledgment to the client
        ackPacket successAck;
        successAck.errorCode = SUCCESS;
        successAck.ack = SUCCESS_ACK;

        if (send(clientSocket, &successAck, sizeof(ackPacket), 0) < 0) {
            perror("Error sending success acknowledgment to client");
            break;  // Exit the loop if there is an error
        }
    }
    printf("Exiting thread...\n"); 

    // Close the client socket when communication is done
    close(clientSocket);

    // Mark the slot as available
    *((int*)arg) = -1;

    pthread_exit(NULL);
}

int main() {
    // Initialize the client fds to -1
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }

    // Create and configure the server socket
    int serverSocket = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (serverSocket < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = SOCKET_FAMILY;
    serverAddr.sin_port = htons(NM_CLT_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error binding socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, MAX_LISTEN_BACKLOG) < 0) {
        perror("Error listening for connections");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    printf("\x1b[32mNaming Server is listening for connections...\x1b[0m\n");

    // Accept incoming connections and spawn a thread for each
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        // Find the first available slot in client_fds
        int clientIndex = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] < 0) {
                clientIndex = i;
                break;
            }
        }

        if (clientIndex == -1) {
            // All slots are in use, handle appropriately (e.g., reject the connection)
            continue;
        }

        // Accept a new connection
        client_fds[clientIndex] = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (client_fds[clientIndex] < 0) {
            perror("Error accepting client connection");
            continue;
        }

        // Spawn a new thread to handle client communication
        if (pthread_create(&clientThreads[clientIndex], NULL, handleClientCommunication, (void*)&client_fds[clientIndex]) != 0) {
            perror("Error creating client communication thread");
            close(client_fds[clientIndex]);
            // Mark the slot as available
            client_fds[clientIndex] = -1;
            continue;
        }

        // Detach the thread to avoid memory leaks
        pthread_detach(clientThreads[clientIndex]);
    }

    // Close the server socket (this won't be reached in this simple example)
    close(serverSocket);

    return 0;
}
