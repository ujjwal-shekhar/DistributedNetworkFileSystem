// nm.c
#include "../utils/headers.h"

int client_fds[MAX_CLIENTS];                    // Make a list of client fds
int server_fds[MAX_SERVERS];                    // Make a list of server fds
pthread_t clientThreads[MAX_CLIENTS];           // List of client threads
pthread_t serverNMThreads[MAX_CLIENTS];         // List of client <-> NM interaction threads
ServerDetails servers[MAX_SERVERS];             // List of servers
sem_t servers_initialized;                      // Semaphore to wait for MIN_SERVERS to come alive before client requests begin

int num_servers_running = 0;                    // Keep track of the number of servers running
sem_t num_servers_running_mutex;                // Binary semaphore to lock the critical section    

// Function to handle client communication in a separate thread
void* handleClientCommunication(void* arg) {
    int clientSocket = *((int*)arg);

    // // Print all the server's information
    // // Print all servers' info
    // sem_wait(&num_servers_running_mutex);
    //     for (int i = 0; i < num_servers_running; i++) {
    //         printf("\nServer ID: %d\n", servers[i].serverID);
    //         printf("Server port_nm: %d\n", servers[i].port_nm);
    //         printf("Server port_client: %d\n", servers[i].port_client);
    //         printf("Server online: %d\n", servers[i].online);
    //     }
    // sem_post(&num_servers_running_mutex);

    while (1) {
        // Add your logic to handle communication with the client
        // For example, you can receive requests, process them, and send acknowledgments
        // Receive a struct Client Request
        ClientRequest clientRequest;
        if (recv(clientSocket, &clientRequest, sizeof(clientRequest), 0) < 0) {
            perror("Error receiving client request");
            break;  // Exit the loop if there is an error
        }

        // Search in the serverDetails to find
        // which storage server has the requested
        // path inside it. Do this for all num_args
        // number of arguments.
        int ss_num = 0; /* Hardcoded for now */

        // Find the server socket based on ss_num
        int serverSocket = -1;

        // Check if ss_num is within the valid range
        if (ss_num >= 0 && ss_num < MAX_SERVERS) {
            // Check if the server is online
            if (servers[ss_num].online) {
                // Check if the Request_type is one in which 
                // we need to send the client details of the SS
                if (
                    clientRequest.requestType == READ_FILE ||
                    clientRequest.requestType == WRITE_FILE ||
                    clientRequest.requestType == GET_FILE_INFO
                ) {
                    // Once you get the storage server details
                    // send ACK bit with ACK_TYPE = CNNCT_TO_SRV_ACK
                    // Handle this interaction in Client <-> Server
                    ackPacket cltAck;
                    cltAck.ack = CNNCT_TO_SRV_ACK;
                    cltAck.errorCode = SUCCESS;
                    if (send(clientSocket, &cltAck, sizeof(cltAck), 0) < 0) {
                        perror("Error sending client request");
                        break;
                    }

                    // Your job is done, continue to accept requests
                } else {
                    // This will be different now, We will send 
                    // the clientRequest ourselves instead of 
                    // sending the client details to the storage
                    // server. The storage server will then
                    // send the ACK bit back to us. We will
                    // then send the ACK bit back to the client.
                    
                    // Send the clientRequest on the serverIP
                    // and port_nm
                    ackPacket cltAck;
                    cltAck.ack = INIT_ACK;
                    cltAck.errorCode = SUCCESS;
                    if (send(clientSocket, &cltAck, sizeof(cltAck), 0) < 0) {
                        perror("Error sending client request");
                        break;
                    }

                    // Send it over
                    
                }

            } else {
                // The server is not online, handle appropriately
                // (e.g., send an error acknowledgment to the client)
                // Send SERVER_OFFLINE with FAILURE_ACK to the client
                // The server is not online, send an error acknowledgment to the client
                ackPacket cltAck;
                printf("CASE 3\n");
                cltAck.ack = FAILURE_ACK;
                cltAck.errorCode = SERVER_OFFLINE;
                if (send(clientSocket, &cltAck, sizeof(cltAck), 0) < 0) {
                    perror("Error sending client request");
                    break;
                }
            }
            
        } else {
            // Invalid ss_num, handle appropriately
            // (e.g., send an error acknowledgment to the client)
            // Send WRONG_PATH with FAILURE_ACK to the client
            ackPacket cltAck;
            cltAck.ack = FAILURE_ACK;
            cltAck.errorCode = WRONG_PATH;
            if (send(clientSocket, &cltAck, sizeof(cltAck), 0) < 0) {
                perror("Error sending client request");
                break;
            }
        }
    }

    // Close the client socket when communication is done
    close(clientSocket);

    // Mark the slot as availablez
    *((int*)arg) = -1;

    pthread_exit(NULL);
}

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

void* listenServerRequests(void* arg) {
    // Placeholder implementation for listening to storage server requests
    // This thread will initialize servers and post to servers_initialized semaphore
    // whenever a new server is initialized

    // Initialize the client fds to -1
    for (int i = 0; i < MAX_SERVERS; i++) {
        servers[i].serverID = -1;
        servers[i].online = false;
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
    serverAddr.sin_port = htons(NM_NEW_SRV_PORT); // Use the appropriate port for the naming server
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

    printf("\x1b[32mNaming Server is listening for SERVER connections...\x1b[0m\n");

    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        // Accept a new connection
        int storageServerSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

        if (storageServerSocket < 0) {
            perror("Error accepting server connection");
            close(storageServerSocket);
            continue;
        }

        // Receive server details
        ServerDetails receivedServerDetails;
        if (recv(storageServerSocket, &receivedServerDetails, sizeof(ServerDetails), 0) < 0) {
            perror("Error receiving server details");
            close(storageServerSocket);
            continue;
        }

        // Search for the server in the servers list
        int serverID = receivedServerDetails.serverID;

        if (servers[serverID].online) {
            // Server is already online, send FAILURE_ACK
            // With error code for server already registered
            ackPacket failureAck;
            failureAck.errorCode = SERVER_ALREADY_REGISTERED;
            failureAck.ack = FAILURE_ACK;

            if (send(storageServerSocket, &failureAck, sizeof(ackPacket), 0) < 0) {
                perror("Error sending failure acknowledgment to server");
            }

            close(storageServerSocket);
        } else {
            // Server is not online, register it
            sem_wait(&num_servers_running_mutex);
                servers[serverID] = receivedServerDetails;

                printf("\nServer Details\n");
                printf("Server ID: %d\n", serverID);
                printf("Server online: %d\n", servers[serverID].online);
                printf("NM Port: %d\n", servers[serverID].port_nm);
                printf("Clt Port: %d\n", servers[serverID].port_client);

                servers[serverID].online = true;
            sem_post(&num_servers_running_mutex);

            // Send SUCCESS_ACK to the server
            ackPacket successAck;
            successAck.errorCode = SUCCESS;
            successAck.ack = SUCCESS_ACK;

            if (send(storageServerSocket, &successAck, sizeof(ackPacket), 0) < 0) {
                perror("Error sending success acknowledgment to server");
            }

            // Post to servers_initialized semaphore
            sem_wait(&num_servers_running_mutex);
                num_servers_running++;
            sem_post(&num_servers_running_mutex);

            if (num_servers_running == NUM_INIT_SERVERS) {
                sem_post(&servers_initialized);
            }

            // Spawn an alive thread
            pthread_t aliveThreadId;
            if (pthread_create(&aliveThreadId, NULL, aliveThreadAsk, NULL) != 0) {
                perror("Error creating aliveThread");
            }

            // Detach the thread to avoid memory leaks
            pthread_detach(aliveThreadId);
        }
    }

    // Close the server socket (this won't be reached in this simple example)
    close(serverSocket);

    return NULL;
}


void* listenClientRequests(void* arg) {
    // Placeholder implementation for listening to client requests
    // Initialize the client fds to -1
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i] = -1;
    }

    printf("Clients listener is woken up\n");

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

    printf("\x1b[32mNaming Server is listening for CLIENT connections...\x1b[0m\n");

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

    return NULL;
}

int main() {
    // Now, a semaphore will be initialized to 
    // minus(NUM_INIT_SERVERS). Every time 
    // a server is initialized in the listen server
    // connection requests, we will post here
    // When it is finally zero, the program will 
    // continue
    sem_init(&servers_initialized, 0, 0);
    sem_init(&num_servers_running_mutex, 0, 1);

    // I will first spawn a thread to listen 
    // for incoming storage server requests
    pthread_t listenServerThreadId;
    if (pthread_create(&listenServerThreadId, NULL, listenServerRequests, NULL) != 0) {
        perror("Error creating listenServerRequests thread");
        exit(EXIT_FAILURE);
    }

    // Wait for the servers to be initialized
    sem_wait(&servers_initialized);

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