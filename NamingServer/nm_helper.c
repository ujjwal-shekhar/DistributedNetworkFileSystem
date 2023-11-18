#include "nm.h"

/**
 * @brief Prints server information to the console.
 * 
 * @param server : ServerDetails struct containing server information.
 */
void printServerInfo(ServerDetails server) {
    printf("\nServer ID: %d\n", server.serverID);
    printf("Server port_nm: %d\n", server.port_nm);
    printf("Server port_client: %d\n", server.port_client);
    printf("Server online: %d\n", server.online);
}

/**
 * @brief Sends an acknowledgment packet to the client.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param ack : Pointer to AckPacket struct containing acknowledgment details.
 */
void sendAckToClient(int* clientSocket, AckPacket* ack) {
    if (send(*clientSocket, ack, sizeof(AckPacket), 0) < 0) {
        perror("Error sending ACK to client");
    }
}

/**
 * @brief Sends server details to the client.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param serverDetails : Pointer to ServerDetails struct containing server details.
 */
void sendServerDetailsToClient(int* clientSocket, ServerDetails* serverDetails) {
    if (send(*clientSocket, serverDetails, sizeof(ServerDetails), 0) < 0) {
        perror("Error sending server details to client");
    }
}

/**
 * @brief Handles the case when the server is offline.
 * 
 * @param clientSocket : Client socket file descriptor.
 */
void handleServerOffline(int* clientSocket) {
    // The server is not online, send an error acknowledgment to the client
    AckPacket cltAck;
    cltAck.ack = FAILURE_ACK;
    cltAck.errorCode = SERVER_OFFLINE;
    sendAckToClient(clientSocket, &cltAck);
    close(*clientSocket);
}

/**
 * @brief Handles the case of an invalid server path.
 * 
 * @param clientSocket : Client socket file descriptor.
 */
void handleWrongPath(int* clientSocket) {
    // Invalid ss_num, send an error acknowledgment to the client
    AckPacket cltAck;
    cltAck.ack = FAILURE_ACK;
    cltAck.errorCode = WRONG_PATH;
    sendAckToClient(clientSocket, &cltAck);
    close(*clientSocket);
}

/**
 * @brief Sends a connection acknowledgment to the client.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param ackType : Type of acknowledgment.
 * @param errorCode : Error code indicating the status of the operation.
 */
void sendConnectionAcknowledgment(int* clientSocket, AckBit ackType, ErrorCode errorCode) {
    printf("Sending a connection acknowledgement\n");
    AckPacket cltAck;
    cltAck.ack = ackType;
    cltAck.errorCode = errorCode;
    sendAckToClient(clientSocket, &cltAck);
}

/**
 * @brief Forwards a client request to the storage server.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param clientRequest : Pointer to ClientRequest struct containing client request details.
 * @param ss_num : Storage server number.
 * @param servers : Pointer to an array of ServerDetails structs containing server details.
 */
void forwardClientRequestToServer(int* clientSocket, ClientRequest* clientRequest, int ss_num, ServerDetails* servers) {
    // Connect to the storage server
    int storage_fd = connectToStorageServer(ss_num, servers);
    if (storage_fd < 0) {
        close(*clientSocket);
        return;
    }

    // Send the clientRequest to the storage server
    if (send(storage_fd, clientRequest, sizeof(ClientRequest), 0) < 0) {
        perror("Can't send clientRequest to storage server");
        close(*clientSocket);
        close(storage_fd);
        return;
    }

    // Receive the acknowledgment from the storage server
    AckPacket nmAck;
    if (recv(storage_fd, &nmAck, sizeof(AckPacket), 0) < 0) {
        perror("Error receiving acknowledgment from storage server");
        close(*clientSocket);
        close(storage_fd);
        return;
    }

    // Forward the acknowledgment to the client
    sendAckToClient(clientSocket, &nmAck);

    printf("Acknowledgment received from storage server: %d\n", ss_num);

    close(storage_fd);
}

/**
 * @brief Connects to the storage server.
 * 
 * @param ss_num : Storage server number.
 * @param servers : Pointer to an array of ServerDetails structs containing server details.
 * 
 * @return Storage server socket file descriptor.
 */
int connectToStorageServer(int ss_num, ServerDetails* servers) {
    int storage_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (storage_fd < 0) {
        perror("Error creating storage socket");
        return -1;
    }

    // Create a sockaddr_in struct for the storage socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SOCKET_FAMILY;
    server_addr.sin_port = htons(servers[ss_num].port_nm);
    server_addr.sin_addr.s_addr = inet_addr(servers[ss_num].serverIP);

    // Connect to the storage server
    if (connect(storage_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Can't connect to storage server");
        close(storage_fd);
        return -1;
    }

    return storage_fd;
}

/**
 * @brief Handles a client request.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param clientRequest : Pointer to ClientRequest struct containing client request details.
 * @param ss_num : Storage server number.
 * @param servers : Pointer to an array of ServerDetails structs containing server details.
 */
void handleClientRequest(int* clientSocket, ClientRequest* clientRequest, int ss_num, ServerDetails* servers) {
    printf("Handling client request\n");
    // Check if ss_num is within the valid range
    if (ss_num >= 0 && ss_num < MAX_SERVERS) {
        // Check if the server is online
        if (servers[ss_num].online) {
            // Check if the Request_type is one in which 
            // we need to send the client details of the SS
            if (
                clientRequest->requestType == READ_FILE ||
                clientRequest->requestType == WRITE_FILE ||
                clientRequest->requestType == GET_FILE_INFO
            ) {
                sendConnectionAcknowledgment(clientSocket, CNNCT_TO_SRV_ACK, SUCCESS);

                // Send the ServerDetails to the client
                sendServerDetailsToClient(clientSocket, &servers[ss_num]);
            } else {
                sendConnectionAcknowledgment(clientSocket, INIT_ACK, SUCCESS);

                // Send the clientRequest to the storage server
                forwardClientRequestToServer(clientSocket, clientRequest, ss_num, servers);
            }

        } else {
            handleServerOffline(clientSocket);
        }
    } else {
        handleWrongPath(clientSocket);
    }
}
