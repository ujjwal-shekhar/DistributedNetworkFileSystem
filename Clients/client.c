#include "client.h"
#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

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
        perror("Error creating socket\n");
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
        perror("Error connecting to server");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

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
            perror("Invalid request syntax\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }
        printf("\nThe request is valid\n");

        /* Handle Server bt */
        // Send the request to the server
        if (send(sock_fd, &clientRequest, sizeof(clientRequest), 0) < 0) {
            perror("Error sending request to server\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        printf("Sent the client request to NM\n");

        // Receive the AckPacket from server
        AckPacket ack;
        if (recv(sock_fd, &ack, sizeof(ack), 0) < 0) {
            perror("Error receiving ack from server\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        printf("Received ack from server : %d : %d\n", ack.ack, ack.errorCode);

        // Check if you got a FAILURE_ACK
        // Inform the error on stdout
        if (ack.ack == FAILURE_ACK) {
            /* Make use of the error codes written */
            printf("There was an error\n");
            close(sock_fd);
            exit(EXIT_FAILURE);
        }

        // Now, since the ack is not FAILURE
        // Check if we need to connect to the server next
        // will the NM serve our request on it'sown
        if (ack.ack == INIT_ACK) { // NM will server our request
            printf("Waiting for NM to reply with status\n");
            // Receive the Job Status from NM
            if (recv(sock_fd, &ack, sizeof(ack), 0) < 0) {
                printf("Error receiving ack from server\n");
                close(sock_fd);
                exit(EXIT_FAILURE);
            }

            // Print the Job Status on stdout to inform the user
            if (ack.ack == SUCCESS_ACK) {
                printf("SS completed your job\n");
            } else {
                printf("Job failed to complete\n");
            }
        } else if (ack.ack == CNNCT_TO_SRV_ACK) { // We must connect to the server
            printf("CNNCT_TO_SRV_ACK received\n");
            // Receive the server details from NM
            ServerDetails server;
            if (recv(sock_fd, &server, sizeof(server), 0) < 0) {
                printf("Error receiving server details from naming server\n");
                close(sock_fd);
                exit(EXIT_FAILURE);
            }

            // Print the server details
            printf("\nServer ID: %d\n", server.serverID);
            printf("Server port_nm: %d\n", server.port_nm);
            printf("Server port_client: %d\n", server.port_client);
            printf("Server online: %d\n", server.online);

            // Create new socket
            int clt_srv_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
            if (clt_srv_fd < 0) {
                printf("Error creating socket\n");
                close(sock_fd);
                close(clt_srv_fd);
                exit(EXIT_FAILURE);
            }

            // Create a sockaddr_in struct for the server
            struct sockaddr_in storage_server_addr;
            memset(&storage_server_addr, 0, sizeof(storage_server_addr));
            storage_server_addr.sin_family = SOCKET_FAMILY;
            storage_server_addr.sin_port = htons(server.port_client);
            storage_server_addr.sin_addr.s_addr = inet_addr(server.serverIP);

            // connect() to the server on given IP and port
            // Connect to the server
            if (connect(clt_srv_fd, (struct sockaddr*) &storage_server_addr, sizeof(storage_server_addr)) < 0) {
                printf("Error connecting to server\n");
                close(sock_fd);
                close(clt_srv_fd);
                exit(EXIT_FAILURE);
            }

            // send() the request
            if (send(clt_srv_fd, &clientRequest, sizeof(clientRequest), 0) < 0) {
                printf("Error sending client request to storage server\n");
                close(sock_fd);
                close(clt_srv_fd);
                exit(EXIT_FAILURE);
            }

            // // Check the request type
            // if (clientRequest.requestType == READ_FILE) {
            //     if (!get_file_data_from_ss(&clt_srv_fd)) {
            //         printf("Error reading file from storage server\n");
            //         close(sock_fd);
            //         close(clt_srv_fd);
            //         exit(EXIT_FAILURE);
            //     }
            // } else if (clientRequest.requestType == WRITE_FILE) {
            //     if (!send_file_data_to_ss(&clt_srv_fd, clientRequest.arg1)) {
            //         printf("Error sending file to storage server\n");
            //         close(sock_fd);
            //         close(clt_srv_fd);
            //         exit(EXIT_FAILURE);
            //     }
            // } else {

            // }

            // Wait for the ack
            if (recv(clt_srv_fd, &ack, sizeof(ack), 0) < 0) {
                printf("Error receiving ack from storage server\n");
                close(sock_fd);
                close(clt_srv_fd);
                exit(EXIT_FAILURE);
            }

            // Print the response ack
            if (ack.ack == SUCCESS_ACK) {
                printf("Success!\n");
            } else {
                printf("Error!\n");
            }

            close(clt_srv_fd);
        }
    }

    // Close the socket
    close(sock_fd);

    return 0;
}