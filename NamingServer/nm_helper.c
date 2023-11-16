# include "../utils/headers.h"

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

    // Mark the slot as availablez
    *((int*)arg) = -1;

    pthread_exit(NULL);
}