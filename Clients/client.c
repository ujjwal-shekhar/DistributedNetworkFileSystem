#include "../utils/headers.h"

/**
 * @brief Check if the user request of the correct format
 * 
 * @param request : the request string
 * @param clientRequest : the client request struct
 * 
 * @return true if valid syntax
 */
bool isValidRequest (char* request, ClientRequest* clientRequest) {
    // Tokenize the string by space
    char* token = strtok(request, " ");

    // Set the RequestType to the first token
    if (strcmp(token, CREATEDIR) == 0) {
        clientRequest->requestType = CREATE_DIR;
    } else if (strcmp(token, CREATEFILE) == 0) {
        clientRequest->requestType = CREATE_FILE;
    } else if (strcmp(token, READFILE) == 0) {
        clientRequest->requestType = READ_FILE;
    } else if (strcmp(token, WRITEFILE) == 0) {
        clientRequest->requestType = WRITE_FILE;
    } else if (strcmp(token, DELETEFILE) == 0) {
        clientRequest->requestType = DELETE_FILE;
    } else if (strcmp(token, DELETEDIR) == 0) {
        clientRequest->requestType = DELETE_DIR;
    } else {
        return false;
    }

    // Set the number of arguments
    clientRequest->num_args = 0;

    // Set the arguments
    while (token != NULL) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            if (clientRequest->num_args == 0) {
                strcpy(clientRequest->arg1, token);
            } else if (clientRequest->num_args == 1) {
                strcpy(clientRequest->arg2, token);
            } else {
                return false;
            }
            clientRequest->num_args++;
        }
    }

    // Return true if the number of arguments is valid
    return (clientRequest->num_args == 2 || clientRequest->num_args == 1);
}

int main() {
    // Create a socket
    int sock_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (sock_fd < 0) {
        printf("Error creating socket\n");
        exit(-1);
    }

    // Create a sockaddr_in struct for the server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SOCKET_FAMILY;
    server_addr.sin_port = htons(NM_CLT_PORT);
    server_addr.sin_addr.s_addr = inet_addr(NM_IP);

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to server\n");
        exit(-1);
    }

    // Initialize client ID
    int clientID = -1;

    while (true) {
        // We will ask the user to input their request
        char request[MAX_REQUEST_SIZE + 1];

        // Read the line
        printf("Enter your request: ");
        fgets(request, MAX_REQUEST_SIZE, stdin);

        // Check if the line is just white space
        // If so, break
        bool isWhiteSpace = true;
        for (int i = 0; i < strlen(request); i++) {
            if (request[i] != ' ' && request[i] != '\n' && request[i] != '\t') {
                isWhiteSpace = false;
                break;
            }
        }
        if (isWhiteSpace) {
            break;
        }

        // Make an empty ClientRequest 
        ClientRequest clientRequest;
        memset(&clientRequest, 0, sizeof(clientRequest));

        // Add the client details
        clientRequest.clientDetails.clientID = -1;

        // Validate the request syntax
        if (!isValidRequest(request, &clientRequest)) {
            printf("Invalid request syntax\n");
            return 0;
        }

        /* Handle Server bt */
        // Send the request to the server
        if (send(sock_fd, &clientRequest, sizeof(clientRequest), 0) < 0) {
            printf("Error sending request to server\n");
            exit(-1);
        }

        // Receive the AckPacket from server
        ackPacket ack;
        if (recv(sock_fd, &ack, sizeof(ack), 0) < 0) {
            printf("Error receiving ack from server\n");
            exit(-1);
        }

        // Check if the ack is a success
        if (ack.ack == SUCCESS_ACK) {
            printf("Request successful\n");
        } else if (ack.ack == FAILURE_ACK) {
            printf("Request failed\n");
        } else if (ack.ack == STOP_ACK) {
            printf("Request failed, server asked to STOP\n");
            break;
        } 
    }

    // Close the socket
    close(sock_fd);

    return 0;
}