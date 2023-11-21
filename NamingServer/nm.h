// nm.h
#ifndef NM_H
#define NM_H

#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

// Function to print server information
void printServerInfo(ServerDetails server);

// Function to send an acknowledgment packet to the client
bool sendAckToClient(int* clientSocket, AckPacket *ack);

// Function to send server details to the client
bool sendServerDetailsToClient(int* clientSocket, ServerDetails *serverDetails);

// Function to handle server offline scenario
void handleServerOffline(int* clientSocket);

// Function to handle wrong path scenario
void handleWrongPath(int* clientSocket);

// Function to send connection acknowledgment to the client
bool sendConnectionAcknowledgment(int* clientSocket, AckBit ackType, ErrorCode errorCode);

// Function to forward client request to the storage server
bool forwardClientRequestToServer(int* clientSocket, ClientRequest *clientRequest, int ss_num, ServerDetails *servers);

// Function to connect to the storage server
bool connectToStorageServer(int* storage_fd, int ss_num, ServerDetails *servers);

// Function to handle client request
bool handleClientRequest(int* clientSocket, ClientRequest *clientRequest, int ss_num, ServerDetails *servers);

// Function to register a new server
bool registerNewServer(
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

// Function to accept a new connection
bool acceptNewConnection(int* storageServerSocket, int* serverSocket, struct sockaddr_in* clientAddr, socklen_t* clientLen);

// Function to receive server details
bool receiveServerDetails(int* storageServerSocket, ServerDetails* receivedServerDetails);

// Function to spawn alive check thread
void spawnAliveThread(void* aliveThreadAsk);

// Function to find the storage server corresponding to the given address
int findStorageServer(const char* address, ServerDetails* servers);

// Function to assign a redundant storage server
void assignRedundantServer(int serverID);

// Function to change activity of redundant storage server
void changeRedundantServerActivity(int serverID, int activeServerID);

// Function to remove inactive redundant storage server
void removeInactiveRedundantServer(int serverID, int inactiveServerID);

// Function to figure out which all servers use the particular server as redundant
void checkWhereRedundant(int serverID);

#endif // NM_H