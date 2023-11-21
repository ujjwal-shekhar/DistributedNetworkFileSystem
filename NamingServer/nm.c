// nm.c
#include "nm.h"
#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

int client_fds[MAX_CLIENTS];                    // Make a list of client fds
int server_fds[MAX_SERVERS];                    // Make a list of server fds
pthread_t clientThreads[MAX_CLIENTS];           // List of client threads
pthread_t serverNMThreads[MAX_CLIENTS];         // List of client <-> NM interaction threads
ServerDetails servers[MAX_SERVERS];             // List of servers
sem_t servers_initialized;                      // Semaphore to wait for MIN_SERVERS to come alive before client requests begin
trienode * root = NULL;                         // Global trie
LRU lru[MAX_CACHE_SIZE];                        // LRU cache             

int num_servers_running = 0;                    // Keep track of the number of servers running
sem_t num_servers_running_mutex;                // Binary semaphore to lock the critical section    

/**
 * @brief Handles communication with a client in a separate thread.
 * 
 * This function runs in a separate thread to handle communication with a client.
 * It receives client requests, identifies the appropriate storage server,
 * and delegates the request handling to the `handleClientRequest` function.
 * The client socket is used to communicate with the Network Manager (NM).
 * 
 * @param arg : Pointer to an integer representing the client socket.
 *              The client socket is extracted from this pointer and used for communication.
 *              After completion, the client socket is closed, and the thread slot is marked as available.
 * 
 * @note The function runs in an infinite loop until an error occurs during client request reception.
 * 
 */
void* handleClientCommunication(void* arg) {
    // Extract the client socket from this 
    // We will use this exact socket to talk
    // to the NM
    int clientSocket = *((int*)arg);

    LOG("Client communication started", true);

    while (1) {
        // Populate a ClientRequest struct by receiving
        // in it from the client.
        ClientRequest clientRequest;
        if (recv(clientSocket, &clientRequest, sizeof(clientRequest), 0) < 0) {
            LOG("Error receiving client request", false);
            break;  // Exit the loop if there is an error
        }

        LOG("Received Client Request", true);

        // Search in the serverDetails to find
        // which storage server has the requested
        // path inside it. Do this for all num_args
        // number of arguments.
        int ss_num = findStorageServer(clientRequest.arg1, root, lru);

        // snprintf to add the ss_num found
        char inform_log[1024];
        snprintf(inform_log, 1024, "Found storage server %d for path %s", ss_num, clientRequest.arg1);
        LOG(inform_log, true);        

        if ((ss_num < 0) || (!handleClientRequest(&clientSocket, &clientRequest, ss_num, servers))) {
            LOG("Failed to process client request", false);
        }
    }

    // Close the client socket when communication is done
    close(clientSocket);

    // Mark the slot as available
    *((int*)arg) = -1;

    pthread_exit(NULL);
}

/**
 * @brief Performs server-alive checks in a separate thread.
 * 
 * This function is intended to run in a separate thread and perform
 * server-alive checks. It sends a "CHECK" packet to the server, receives
 * a "CHECK" packet along with the serverID, and repeats the process every
 * 10 seconds. The thread execution can be terminated by the server.
 * 
 * @param arg : Unused parameter, required by the pthread_create function.
 * 
 * @note The function runs in an infinite loop with a sleep of 10 seconds between iterations.
 * 
 * @return Always returns NULL.
 */
void* aliveThreadAsk(void* arg) {
    // Placeholder implementation for aliveThread
    while (1) {
        // Send "CHECK" packet to the server
        // Receive "CHECK" packet + serverID from the server
        // Sleep for 10 seconds
        // sleep(10);
        break;
    }
    return NULL;
}

/**
 * @brief Listens to incoming storage server requests and handles server registration.
 * 
 * This function initializes server details, creates and configures the server socket, and enters
 * an infinite loop to listen for incoming connections. Upon accepting a connection, it receives
 * server details and registers the new server using the `registerNewServer` function.
 * After processing, it closes the server socket.
 * 
 * @param arg : Unused parameter (required for pthread_create).
 * 
 * @note The function runs indefinitely, listening for incoming storage server requests.
 * 
 */ 
void* listenServerRequests(void* arg) {
    initializeServerDetails(servers);

    // Make a socket fd for the Naming Server
    int serverSocket = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (serverSocket < 0) {
        LOG("Error creating socket", false);
        exit(EXIT_FAILURE);
    }
    
    // Initialize the port details 
    // that we need to bind the listener (us) on
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = SOCKET_FAMILY;
    serverAddr.sin_port = htons(NM_NEW_SRV_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket fd to the port 
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG("Error binding socket", false);
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    LOG("Naming Server is bound on the NM_NEW_SRV_PORT", true);
    
    // Start queuing everything that is listened to 
    if (listen(serverSocket, MAX_LISTEN_BACKLOG) < 0) {
        LOG("Error listening for connections", false);
        close(serverSocket); 
        exit(EXIT_FAILURE);
    }

    // Log that the client listener has started to listen
    LOG("Naming Server is listening for SERVER connections", true);

    while (1) {
        // Make the socket address struct 
        // Mostly obsolete since we don't need
        // to bind this to anyone
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        // Send the pointer to the storage server address 
        // structure to get accepted
        int storageServerSocket;
        if (!acceptNewConnection(&storageServerSocket, &serverSocket, &clientAddr, &clientLen)) {
            continue; // Failed to connect
        }
        LOG("Connection established with storage server", true);

        // ServerDetails struct to be populated by 
        // receiving from the server.
        ServerDetails receivedServerDetails;
        if (!receiveServerDetails(&storageServerSocket, &receivedServerDetails)) {
            close(storageServerSocket); // Since we failed to receive, close the socket
            continue; // Failed to receive server details
        }
        LOG("Received server details", true);
        printf("ONLINE : %d : %s\n", receivedServerDetails.serverID, ((receivedServerDetails.online) ? "YES" : "NO"));

        if (!registerNewServer(
            servers,
            &num_servers_running_mutex,
            &servers_initialized,
            &storageServerSocket,
            server_fds,
            &num_servers_running,
            aliveThreadAsk,
            &receivedServerDetails,  // Pass receivedServerDetails to the function,
            &root
        )) {
            // Failed to register
            close(storageServerSocket);
            continue;
        }

    }

    closeServerSocket(&serverSocket);

    return NULL;
}


void* listenClientRequests(void* arg) {
    // Placeholder implementation for listening to client requests
    // Initialize the client fds to -1
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }

    LOG("Initialized client fds", true);

    // Create and configure the server socket
    int serverSocket = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (serverSocket < 0) {
        LOG("Error creating socket", false);
        exit(EXIT_FAILURE);
    }

    LOG("Created Naming server socket for client new clients", true);

    // Configure server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = SOCKET_FAMILY;
    serverAddr.sin_port = htons(NM_CLT_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the address
    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG("Error binding socket", false);
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    LOG("Bound Naming server socket on NM_CLT_PORT", true);

    // Listen for incoming connections
    if (listen(serverSocket, MAX_LISTEN_BACKLOG) < 0) {
        LOG("Error listening for connections", false);
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Log that the client listener has started to listen
    LOG("Naming Server is listening for CLIENT connections", true);

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
            LOG("Available slot not found in client_fds", false);
            continue;
        } else {
            LOG("Available slot found in client_fds", true);
        }

        // Accept a new connection
        client_fds[clientIndex] = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (client_fds[clientIndex] < 0) {
            LOG("Error accepting client connection", false);
            continue;
        } else {
            LOG("Accepted connection from server", true);
        }

        // Spawn a new thread to handle client communication
        if (pthread_create(&clientThreads[clientIndex], NULL, handleClientCommunication, (void*)&client_fds[clientIndex]) != 0) {
            LOG("Error creating client communication thread", false);
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

    return NULL;
}

int main() {
    // A semaphore will be initialized to 
    // minus(NUM_INIT_SERVERS). Every time 
    // a server is initialized in the listen server
    // connection requests, we will post here
    // When it is finally zero, the program will 
    // continue
    sem_init(&servers_initialized, 0, 0);
    sem_init(&num_servers_running_mutex, 0, 1);

    // Initialize the cache
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        lru[i].rank = 1e9;
        lru[i].serverID = -1;
        lru[i].pathHash = 0;
    }

    // Log that the NM file is running
    LOG("NM file is running", true);

    // I will first spawn a thread to listen 
    // for incoming storage server requests
    pthread_t listenServerThreadId;
    if (pthread_create(&listenServerThreadId, NULL, listenServerRequests, NULL) != 0) {
        perror("Error creating listenServerRequests thread");
        exit(EXIT_FAILURE);
    }

    // Log that the Server listener was spawned
    LOG("Server listener started", true);

    // Wait for the servers to be initialized
    sem_wait(&servers_initialized);

    // Log that the Client listener was spawned
    LOG("Client listener started", true);

    // Spawn a thread to forever listen for new
    // client requests.
    pthread_t listenClientThreadId;
    if (pthread_create(&listenClientThreadId, NULL, listenClientRequests, NULL) != 0) {
        perror("Error creating listenClientRequests thread");
        exit(EXIT_FAILURE);
    }

    // Wait for user input to exit
    getchar();

    // Close threads and perform cleanup
    pthread_cancel(listenServerThreadId);
    pthread_cancel(listenClientThreadId);
    sem_destroy(&servers_initialized);

    return 0;
}