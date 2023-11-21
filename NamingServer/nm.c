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
bool already_seen_servers[MAX_SERVERS];         // List of servers that have already been seen
RedundantServerInfo redundantServers[MAX_SERVERS]; // List of redundant servers
sem_t servers_initialized;                      // Semaphore to wait for MIN_SERVERS to come alive before client requests begin
trienode * root = NULL;                         // Global trie
LRU lru[MAX_CACHE_SIZE];                        // LRU cache             

int num_servers_running = 0;                    // Keep track of the number of servers running
int num_servers_online = 0;                     // Keep track of the number of servers online
sem_t num_servers_running_mutex;                // Binary semaphore to lock the critical section
sem_t num_servers_online_mutex;                 // Binary semaphore to lock the critical section

int serverAliveSocket;                          // Socket to listen for server alive messages

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

        if ((ss_num < 0) || (!handleClientRequest(&clientSocket, &clientRequest, ss_num, servers, root))) {
            LOG("Failed to process client request", false);
            if (!sendConnectionAcknowledgment(&clientSocket, FAILURE_ACK, INVALID_INPUT_ERROR)) {
                LOG("Connection acknowledgement failed", false);
            } else {
                LOG("Connection acknowledgement succeeded", true);
            }
        }
    }

    // Close the client socket when communication is done
    close(clientSocket);

    // Mark the slot as available
    *((int*)arg) = -1;

    pthread_exit(NULL);
}

void checkWhereRedundant(int serverID) {
    for (int i = 0; i < MAX_SERVERS; i++) {
        if (servers[i].online) {
            for (int j = 0; j < redundantServers[i].num_active; j++) {
                if (redundantServers[i].active[j] == serverID) {
                    changeRedundantServerActivity(i, serverID);
                    assignRedundantServer(i);
                }
            }
        }
    }
}

void respawnGetNewRedundant(int serverID) {
    int downServers[10];
    int num_down = 0;
    for (int i = 0; i < redundantServers[serverID].num_active; i++) {
        if (!servers[redundantServers[serverID].active[i]].online) {
            downServers[num_down++] = redundantServers[serverID].active[i];
        }
    }

    for (int i = 0; i < num_down; i++) {
        changeRedundantServerActivity(serverID, downServers[i]);
        assignRedundantServer(serverID);
    }
}

void findInactiveRedundant(int serverID) {
    for (int i = 0; i < MAX_SERVERS; i++) {
        for (int j = 0; j < redundantServers[i].num_inactive; j++) {
            if (redundantServers[i].inactive[j] == serverID) {
                removeInactiveRedundantServer(i, serverID);
            }
        }
    }
}

void assignRedundantServer(int serverID) {
    if (redundantServers[serverID].num_active == MAX_REDUN) return;
    int availableServers[10];
    int num_available = 0;
    for (int i = 0; i < MAX_SERVERS; i++) {
        if (servers[i].online && i != serverID) {
            int flag = 1;
            for (int j = 0; j < redundantServers[serverID].num_active; j++) {
                if (redundantServers[serverID].active[j] == i) {
                    flag = 0;
                    break;
                }
            }
            if (flag) {
                availableServers[num_available++] = i;
            }
        }
    }
    int num_to_choose = MAX_REDUN - redundantServers[serverID].num_active;
    for (int i = 0; i < num_to_choose; i++) {
        int index = rand() % num_available;
        redundantServers[serverID].active[redundantServers[serverID].num_active++] = availableServers[index];
        for (int j = index; j < num_available - 1; j++) {
            availableServers[j] = availableServers[j + 1];
        }
        num_available--;
    }
}


void changeRedundantServerActivity(int serverID, int activeServerID) {
    for (int i = 0; i < redundantServers[serverID].num_active; i++) {
        if (redundantServers[serverID].active[i] == activeServerID) {
            for (int j = i; j < redundantServers[serverID].num_active - 1; j++) {
                redundantServers[serverID].active[j] = redundantServers[serverID].active[j + 1];
            }
            redundantServers[serverID].num_active--;
            break;
        }
    }
    redundantServers[serverID].inactive[redundantServers[serverID].num_inactive++] = activeServerID;
}

void removeInactiveRedundantServer(int serverID, int inactiveServerID) {
    for (int i = 0; i < redundantServers[serverID].num_inactive; i++) {
        if (redundantServers[serverID].inactive[i] == inactiveServerID) {
            for (int j = i; j < redundantServers[serverID].num_inactive - 1; j++) {
                redundantServers[serverID].inactive[j] = redundantServers[serverID].inactive[j + 1];
            }
            redundantServers[serverID].num_inactive--;
            break;
        }
    }
}

/**
 * @brief Performs server-alive checks in a separate thread.
 *
 * This function is intended to run in a separate thread and perform
 * server-alive checks. It first receives the serverID from the storage
 * server and then enters an infinite loop to receive alive messages
 * every 10 seconds and update the last ping time for the server.
 *
 * @param arg : Unused parameter, required by the pthread_create function.
 *
 * @note The function runs in an infinite loop where is waits to receive ALIVE
 *
 * @return Always returns NULL.
 */
void* aliveThreadAsk(void* arg) {
    int serverID = 0;
    ssize_t bytes_read;
    char buffer[10], printBuffer[100];
    // Make the socket address struct
    // Mostly obsolete since we don't need
    // to bind this to anyone
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    // Send the pointer to the storage server address
    // structure to get accepted
    int storageServerSocket;
    if (!acceptNewConnection(&storageServerSocket, &serverAliveSocket, &clientAddr, &clientLen)) {
        return NULL; // Failed to connect
    }
    LOG("Connection established with storage server for alive messages", true);

    // Get serverID from storage server
    // Empty buffer
    memset(buffer, '\0', sizeof(buffer));
    bytes_read = recv(storageServerSocket, buffer, sizeof(buffer), 0);
    if (bytes_read <= 0) {
        LOG("Error receiving from storage server", false);
        return NULL;
    }
    sscanf(buffer, "ID%d", &serverID);

    while(1) {
        memset(buffer, '\0', sizeof(buffer));
        bytes_read = recv(storageServerSocket, buffer, sizeof(buffer), 0);
        if (bytes_read <= 0) {
            // Got error or connection closed by client
            if (bytes_read == 0) {
                // Connection closed
                if (servers[serverID].online){
                    servers[serverID].online = false;
                    checkWhereRedundant(serverID);
                    num_servers_online--;
                    memset(printBuffer, '\0', sizeof(printBuffer));
                    sprintf(printBuffer, "Storage server %d disconnected", serverID);
                    LOG(printBuffer, false);
                }
            } else {
                LOG("Error receiving from storage server", false);
            }
            break;
        }
        else {
            memset(printBuffer, '\0', sizeof(printBuffer));
            sprintf(printBuffer, "Received alive message from storage server %d", serverID);
            LOG(printBuffer, true);
        }
    }

    close(storageServerSocket);
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

    bool init_servers_received = false;

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

        if (init_servers_received) {
            if (servers[receivedServerDetails.serverID].online) {
                if (already_seen_servers[receivedServerDetails.serverID]) {
                    respawnGetNewRedundant(receivedServerDetails.serverID);
                    findInactiveRedundant(receivedServerDetails.serverID);
                }
                else {
                    assignRedundantServer(receivedServerDetails.serverID);
                }
            }
        }
        else if (num_servers_running == NUM_INIT_SERVERS) {
            for (int i = 0; i < MAX_SERVERS; i++) {
                if (servers[i].online) {
                    assignRedundantServer(i);
                }
            }
            init_servers_received = true;
        }

        already_seen_servers[receivedServerDetails.serverID] = true;
    }

    closeServerSocket(&serverSocket);

    return NULL;
}

void* printRedundantServers(void* arg) {
    while (1) {
        for (int i = 0; i < MAX_SERVERS; i++) {
            if (servers[i].online) {
                printf("Server %d :\nActive : ", i);
                for (int j = 0; j < redundantServers[i].num_active; j++) {
                    printf("%d ", redundantServers[i].active[j]);
                }
                printf("\n");
                printf("Inactive : ");
                for (int j = 0; j < redundantServers[i].num_inactive; j++) {
                    printf("%d ", redundantServers[i].inactive[j]);
                }
                printf("\n");
            }
        }
        sleep(20);
    }
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
    sem_init(&num_servers_online_mutex, 0, 1);

    srand(time(NULL));

    for (int i = 0; i < MAX_SERVERS; i++) {
        already_seen_servers[i] = false;
    }

    for (int i = 0; i < MAX_SERVERS; i++) {
        memset(redundantServers[i].active, -1, sizeof(redundantServers[i].active));
        memset(redundantServers[i].inactive, -1, sizeof(redundantServers[i].inactive));
        redundantServers[i].num_active = 0;
        redundantServers[i].num_inactive = 0;
    }

    // Initialize the cache
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        lru[i].rank = 1e9;
        lru[i].serverID = -1;
        lru[i].pathHash = 0;
    }

    // Log that the NM file is running
    LOG("NM file is running", true);

    // Make a socket fd for the Alive Checker
    serverAliveSocket = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (serverAliveSocket < 0) {
        LOG("Error creating socket", false);
        exit(EXIT_FAILURE);
    }

    // Initialize the port details
    // that we need to bind the listener (us) on
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = SOCKET_FAMILY;
    serverAddr.sin_port = htons(NM_ALIVE_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket fd to the port
    if (bind(serverAliveSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        LOG("Error binding socket", false);
        close(serverAliveSocket);
        exit(EXIT_FAILURE);
    }
    LOG("Naming Server checks for alive servers on the NM_ALIVE_PORT", true);

    // Start queuing everything that is listened to
    if (listen(serverAliveSocket, MAX_LISTEN_BACKLOG) < 0) {
        LOG("Error listening for connections", false);
        close(serverAliveSocket);
        exit(EXIT_FAILURE);
    }

    // Log that the client listener has started to listen
    LOG("Naming Server is listening for Alive SERVER connections", true);

    // Spawn a thread to listen
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

    pthread_t printRedundantServersThreadId;
    if (pthread_create(&printRedundantServersThreadId, NULL, printRedundantServers, NULL) != 0) {
        perror("Error creating printRedundantServers thread");
        exit(EXIT_FAILURE);
    }

    // Spawn a thread to forever listen for new
    // client requests.
    pthread_t listenClientThreadId;
    if (pthread_create(&listenClientThreadId, NULL, listenClientRequests, NULL) != 0) {
        perror("Error creating listenClientRequests thread");
        exit(EXIT_FAILURE);
    }

    // Log that the Client listener was spawned
    LOG("Client listener started", true);

    // Wait for user input to exit
    getchar();

    // Close threads and perform cleanup
    pthread_cancel(listenServerThreadId);
    pthread_cancel(listenClientThreadId);
    sem_destroy(&servers_initialized);

    return 0;
}