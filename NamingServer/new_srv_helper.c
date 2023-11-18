#include "nm.h"
 
/**
 * @brief Registers a new storage server with the naming server.
 * 
 * This function registers a new storage server by extracting the server ID from received details.
 * It checks if the server is already online and sends an acknowledgment accordingly.
 * If the server is not online, it registers the new server, sends a SUCCESS_ACK, and increments
 * the number of running servers. If all initial servers are running, it posts to the servers_initialized semaphore.
 * Additionally, it spawns an alive thread to monitor the new server's status.
 * 
 * @param servers : Array of ServerDetails representing storage servers.
 * @param num_servers_running_mutex : Semaphore to control access to the number of running servers.
 * @param servers_initialized : Semaphore to signal that all initial servers are running.
 * @param storageServerSocket : Socket for the new storage server.
 * @param server_fds : Array of server sockets.
 * @param num_servers_running : Pointer to the number of running servers.
 * @param receivedServerDetails : Details of the new storage server.
 * 
 * @return true if successful, false otherwise
 */ 
bool registerNewServer(
    ServerDetails* servers,
    sem_t* num_servers_running_mutex,
    sem_t* servers_initialized,
    int* storageServerSocket,
    int* server_fds,
    int* num_servers_running,
    void * aliveThreadAsk,
    ServerDetails* receivedServerDetails
) {
    // Extract server ID from received details
    int serverID = receivedServerDetails->serverID;

    LOG("Registering server", true);
    LOG_SERVER_DETAILS(receivedServerDetails);

    // Check if the server is already online
    if (servers[serverID].online) {
        AckPacket ack;
        ack.errorCode = SERVER_ALREADY_REGISTERED;
        ack.ack = FAILURE_ACK;
        sendAckToClient(storageServerSocket, &ack);
        LOG("Server was already registered11", false);
        return false;
    } else { 
        // Register the new server
        sem_wait(num_servers_running_mutex);
            servers[serverID] = *receivedServerDetails;
            servers[serverID].online = true;
            server_fds[serverID] = *storageServerSocket;
        sem_post(num_servers_running_mutex);

        // Send SUCCESS_ACK to the server
        AckPacket ack;
        ack.errorCode = SUCCESS;
        ack.ack = SUCCESS_ACK;
        if (!sendAckToClient(storageServerSocket, &ack)) {
            return false;
        }

        LOG("Sent ACK packet to storage server", true);

        // Increment the number of running servers
        sem_wait(num_servers_running_mutex);
            (*num_servers_running)++;
        sem_post(num_servers_running_mutex);

        // If all initial servers are running, post to the servers_initialized semaphore
        if (*num_servers_running == NUM_INIT_SERVERS) {
            LOG("All initial servers are running", true);
            sem_post(servers_initialized);
        }
        
        LOG("Server registered", true);
        LOG_SERVER_DETAILS(receivedServerDetails);

        // Spawn an alive thread
        spawnAliveThread(aliveThreadAsk);
    }
    return true;
}

/**
 * @brief Closes the server socket.
 * 
 * This function closes the server socket. Note that in this simple example, it won't be reached.
 * 
 * @param serverSocket : The server socket to be closed.
 * 
 */ 
void closeServerSocket(int* serverSocket) {
    // Close the server socket (this won't be reached in this simple example)
    close(*serverSocket);
}

/**
 * @brief Initializes server details.
 * 
 * This function initializes an array of ServerDetails, setting serverID to -1 and online to false.
 * 
 * @param servers : Array of ServerDetails representing storage servers.
 * 
 */ 
void initializeServerDetails(ServerDetails* servers) {
    for (int i = 0; i < MAX_SERVERS; i++) {
        servers[i].serverID = -1;
        servers[i].online = false;
    }
}

/**
 * @brief Accepts a new connection.
 * 
 * This function accepts a new connection on the provided server socket and returns the
 * socket descriptor for the new connection.
 * 
 * @param serverSocket : The server socket on which to accept connections.
 * @param clientAddr : Pointer to a sockaddr_in struct to store client address information.
 * @param clientLen : Pointer to the length of the client address structure.
 * 
 * @return The socket descriptor for the new connection.
 * 
 */ 
bool acceptNewConnection(int* storageServerSocket, int* serverSocket, struct sockaddr_in* clientAddr, socklen_t* clientLen) {
    *storageServerSocket = accept(*serverSocket, (struct sockaddr*)clientAddr, clientLen);
    if (storageServerSocket < 0) {
        LOG("Error accepting server connection", false);
        return false;
    }
    return true;
}

/**
 * @brief Receives server details.
 * 
 * This function receives server details on the provided socket and stores them in the
 * receivedServerDetails structure.
 * 
 * @param storageServerSocket : The socket from which to receive server details.
 * @param receivedServerDetails : Pointer to a ServerDetails struct to store received details.
 * 
 * @return true if server details were received, else false
 */ 
bool receiveServerDetails(int* storageServerSocket, ServerDetails* receivedServerDetails) {
    if (recv(*storageServerSocket, receivedServerDetails, sizeof(ServerDetails), 0) < 0) {
        LOG("Error receiving server details", false);
        return false;
    }
    LOG("Received server details", true);
    return true;
}

/**
 * @brief Spawns an alive thread to perform server-alive checks.
 * 
 * This function creates a new thread using pthread_create to execute
 * the `aliveThreadAsk` function. The created thread is detached to avoid
 * memory leaks when it completes its execution.
 * 
 * @note The alive thread periodically sends "CHECK" packets to the server
 * and receives responses to ensure server-alive status.
 */
void spawnAliveThread(void* aliveThreadAsk) {
    pthread_t aliveThreadId;
    if (pthread_create(&aliveThreadId, NULL, aliveThreadAsk, NULL) != 0) {
        perror("Error creating aliveThread");
    }

    // Detach the thread to avoid memory leaks
    pthread_detach(aliveThreadId);
}
