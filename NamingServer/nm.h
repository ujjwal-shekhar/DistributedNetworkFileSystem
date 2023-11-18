// nm.h
#ifndef NM_H
#define NM_H

#include "../utils/headers.h"

// Function to print server information
void printServerInfo(ServerDetails server);

// Function to send an acknowledgment packet to the client
void sendAckToClient(int* clientSocket, AckPacket *ack);

// Function to send server details to the client
void sendServerDetailsToClient(int* clientSocket, ServerDetails *serverDetails);

// Function to handle server offline scenario
void handleServerOffline(int* clientSocket);

// Function to handle wrong path scenario
void handleWrongPath(int* clientSocket);

// Function to send connection acknowledgment to the client
void sendConnectionAcknowledgment(int* clientSocket, AckBit ackType, ErrorCode errorCode);

// Function to forward client request to the storage server
void forwardClientRequestToServer(int* clientSocket, ClientRequest *clientRequest, int ss_num, ServerDetails *servers);

// Function to connect to the storage server
int connectToStorageServer(int ss_num, ServerDetails *servers);

// Function to handle client request
void handleClientRequest(int* clientSocket, ClientRequest *clientRequest, int ss_num, ServerDetails *servers);

// Function to register a new server
void registerNewServer(
    ServerDetails* servers,
    sem_t* num_servers_running_mutex,
    sem_t* servers_intialised,
    int* storageServerSocket,
    int* server_fds,
    int* num_servers_running,
    void * aliveThreadAsk,
    ServerDetails* receivedServerDetails
);

// Function to close the server socket
void closeServerSocket(int* serverSocket);

// Function to initialize server details
void initializeServerDetails(ServerDetails* servers);

// Function to create and configure the server socket
void createAndConfigureServerSocket(int *serverSocket);

// Function to accept a new connection
int acceptNewConnection(int* serverSocket, struct sockaddr_in* clientAddr, socklen_t* clientLen);

// Function to receive server details
void receiveServerDetails(int* storageServerSocket, ServerDetails* receivedServerDetails);

// Function to spawn alivee check thread
void spawnAliveThread(void* aliveThreadAsk);

#endif // NM_H