// server.c
#include "server.h"
#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

#define MY_SRV_IP "127.0.0.1"

ServerDetails serverDetails;
sem_t serverDetails_mutex;          // Binary semaphore to atomically carry out priviliedged instructions
rwlock rw_locks[MAX_PATHS];         // Reader writer lock to protect each file

void* aliveThreadReply(void* arg) {
    char buffer[10];

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
    server_addr.sin_port = htons(NM_ALIVE_PORT);
    server_addr.sin_addr.s_addr = inet_addr(NM_IP);

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to server\n");
        exit(EXIT_FAILURE);
    }

    // Send the serverID to the NM
    sprintf(buffer, "ID%d", serverDetails.serverID);
    if (send(sock_fd, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending serverID to NM");
        exit(EXIT_FAILURE);
    }

    // Keep sending "ALIVE" to the NM
    // after every 10 seconds
    while(1) {
        if (send(sock_fd, "ALIVE", strlen("ALIVE"), 0) < 0) {
            perror("Error sending ALIVE to NM");
            exit(EXIT_FAILURE);
        }
        sleep(10);
    }

    close(sock_fd);
    return NULL;
}

void* nmThread(void* arg) {
    // Create a socket
    int sock_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);

    // Create a sockaddr_in struct for the server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SOCKET_FAMILY;
    server_addr.sin_port = htons(serverDetails.port_nm);
    server_addr.sin_addr.s_addr = INADDR_ANY;


    // Bind the socket to the port
    if (bind(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sock_fd, MAX_LISTEN_BACKLOG) < 0) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in nm_addr;
    socklen_t nm_addr_len = sizeof(nm_addr);

    while (1) {
        // Accept a connection request
        int nmSocket = accept(sock_fd, (struct sockaddr*) &nm_addr, &nm_addr_len);
        if (nmSocket < 0) {
            perror("Error accepting connection");
            exit(EXIT_FAILURE);
        }

        // Receive clientRequest
        ClientRequest clientRequest;
        if (recv(nmSocket, &clientRequest, sizeof(ClientRequest), 0) < 0) {
            perror("Error receiving client request");
            exit(EXIT_FAILURE);
        }

        // Remove the "/" at the beginning"
        if (clientRequest.arg1[0] == '/') {
            memmove(clientRequest.arg1, clientRequest.arg1 + 1, strlen(clientRequest.arg1));
        }

        // Initialize the ACK packet
        AckPacket nmAck;
        nmAck.errorCode = SUCCESS;
        nmAck.ack = SUCCESS_ACK;

        // Process clientRequest
        sem_wait(&serverDetails_mutex);

            if (clientRequest.requestType == CREATE_DIR) {
                if (!createDirectory(clientRequest.arg1)) {
                    nmAck.errorCode = OTHER;
                    nmAck.ack = FAILURE_ACK;
                }
            } else if (clientRequest.requestType == CREATE_FILE) {
                if (!createFile(clientRequest.arg1))  {
                    nmAck.errorCode = OTHER;
                    nmAck.ack = FAILURE_ACK;
                }
            } else if (clientRequest.requestType == DELETE_DIR) {
                printf("Delete directory\n");
            } else if (clientRequest.requestType == DELETE_FILE) {
                if (!deleteFile(clientRequest.arg1))  {
                    nmAck.errorCode = OTHER;
                    nmAck.ack = FAILURE_ACK;
                }
            }

            // Add accessible paths again
            serverDetails.num_paths = 0;
            listFilesAndEmptyFolders(".", &serverDetails);

        sem_post(&serverDetails_mutex);

        // Send SUCCESS ACK to NM
        if (send(nmSocket, &nmAck, sizeof(AckPacket), 0) < 0) {
            perror("Error sending ack packet to NM");
            exit(EXIT_FAILURE);
        }

        // Send new serverDetails to NM
        if (send(nmSocket, &serverDetails, sizeof(ServerDetails), 0) < 0) {
            perror("Error sending server details to NM");
            exit(EXIT_FAILURE);
        }
    }
    return NULL;
}

void* clientThread(void* arg) {
    // Create a socket
    int sock_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);

    // Create a sockaddr_in struct for the server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SOCKET_FAMILY;
    server_addr.sin_port = htons(serverDetails.port_client);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the port
    if (bind(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(sock_fd, MAX_LISTEN_BACKLOG) < 0) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in clt_addr;
    socklen_t clt_addr_len = sizeof(clt_addr);


    while (1) {
        // Wait for a connection request
        // Accept a connection request
        int cltSocket = accept(sock_fd, (struct sockaddr*) &clt_addr, &clt_addr_len);
        if (cltSocket < 0) {
            perror("Error accepting connection");
            exit(EXIT_FAILURE);
        }

        // Recv client request
        ClientRequest clientRequest;
        if (recv(cltSocket, &clientRequest, sizeof(ClientRequest), 0) < 0) {
            perror("Error recv client request");
            exit(EXIT_FAILURE);
        }

        // Make the Ack bit ready
        AckPacket ack;
        ack.errorCode = SUCCESS;
        ack.ack = SUCCESS_ACK;

        // Find the index of the path in the list of accessible paths
        int pathIndex = -1;
        for (int i = 0; i < serverDetails.num_paths; i++) {
            if (strcmp(serverDetails.accessible_paths[i], clientRequest.arg1) == 0) {
                pathIndex = i;
                break;
            }
        }

        if (pathIndex == -1) {
            ack.errorCode = INVALID_INPUT_ERROR;
            ack.ack = FAILURE_ACK;

            // Send the ack bit to client
            if (send(cltSocket, &ack, sizeof(ack), 0) < 0) {
                printf("Error sending ack to client\n");
                exit(-1);
            } else {
                printf("Sent the ack bit to client\n");
            }

            close(cltSocket);

            continue;
        }

        // Remove the "/" at the beginning"
        if (clientRequest.arg1[0] == '/') {
            memmove(clientRequest.arg1, clientRequest.arg1 + 1, strlen(clientRequest.arg1));
        }

        // Print the response type
        if (clientRequest.requestType == READ_FILE) {
            sem_wait(&serverDetails_mutex);
                acquire_readlock(&rw_locks[pathIndex]);
                    printf("Read file: %s\n", clientRequest.arg1);
                    if (!read_file_in_ss(clientRequest.arg1, &cltSocket)) {
                        ack.errorCode = OTHER;
                        ack.ack = FAILURE_ACK;
                    }
                release_readlock(&rw_locks[pathIndex]);
            sem_post(&serverDetails_mutex);
        } else if (clientRequest.requestType == WRITE_FILE) {
            sem_wait(&serverDetails_mutex);
                acquire_writelock(&rw_locks[pathIndex]);
                    printf("Write file: %s\n", clientRequest.arg1);
                    if (!write_file_in_ss(clientRequest.arg1, &cltSocket)) {
                        ack.errorCode = OTHER;
                        ack.ack = FAILURE_ACK;
                    }
                release_writelock(&rw_locks[pathIndex]);
            sem_post(&serverDetails_mutex);
        } else if (clientRequest.requestType == GET_FILE_INFO) {
            sem_wait(&serverDetails_mutex);
                acquire_readlock(&rw_locks[pathIndex]);
                    printf("Get file info of : %s\n", clientRequest.arg1);
                    if (!sendFileInformation(clientRequest.arg1, &cltSocket)) {
                        ack.errorCode = OTHER;
                        ack.ack = FAILURE_ACK;
                    }
                release_readlock(&rw_locks[pathIndex]);
            sem_post(&serverDetails_mutex);
        }

        // Send the ack bit to client
        if (send(cltSocket, &ack, sizeof(ack), 0) < 0) {
            printf("Error sending ack to client\n");
            exit(-1);
        } else {
            printf("Sent the ack bit to client\n");
        }

        close(cltSocket);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <serverID> <CLT_PORT> <NM_PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize the semaphores and locks
    sem_init(&serverDetails_mutex, 0, 1);
    for (int i = 0; i < MAX_PATHS; i++) {
        rw_locks[i].readers = 0;
        sem_init(&rw_locks[i].lock, 0, 1);
        sem_init(&rw_locks[i].writeLock, 0, 1);
    }

    // Make a ServerDetails with the given serverID
    sem_wait(&serverDetails_mutex);
        serverDetails.serverID = atoi(argv[1]);
        strcpy(serverDetails.serverIP, MY_SRV_IP); // Replace with your actual IP
        serverDetails.port_client = atoi(argv[2]);
        serverDetails.port_nm = atoi(argv[3]);
        serverDetails.online = 1;
    sem_post(&serverDetails_mutex);

    printf("ONLINE : %s\n", ((serverDetails.online) ? "YES" : "NO"));

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
    server_addr.sin_port = htons(NM_NEW_SRV_PORT);
    server_addr.sin_addr.s_addr = inet_addr(NM_IP);

    // Add accessible paths
    serverDetails.num_paths = 0;
    listFilesAndEmptyFolders(".", &serverDetails);

    // Print the list of accessible paths
    for (int i = 0; i < serverDetails.num_paths; i++) {
        printf("%s\n", serverDetails.accessible_paths[i]);
    }

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to server\n");
        exit(EXIT_FAILURE);
    }

    // Send the server details to the NM
    if (send(sock_fd, &serverDetails, sizeof(ServerDetails), 0) < 0) {
        perror("Error sending server details to NM");
        exit(EXIT_FAILURE);
    }

    // The NM sends back an ack packet
    // Get the server ID assigned by NM to you
    // Store this server ID here
    AckPacket nmAck;
    
    // Receive the ack packet
    if (recv(sock_fd, &nmAck, sizeof(AckPacket), 0) < 0) {
        perror("Error receiving ack packet");
        exit(EXIT_FAILURE);
    }

    // If this is a FAILURE_ACK, just die
    if (nmAck.ack == FAILURE_ACK) {
        printf("Error: NM returned FAILURE_ACK\n");
        exit(EXIT_FAILURE);
    }

    // Now, spawn an aliveThread. This thread 
    // receives a "CHECK" packet from
    // the server, and replies with a "CHECK"
    // packet + serverID. Sleep for 10 seconds.
    // Don't wait for this thread to join
    pthread_t aliveThreadId;
    if (pthread_create(&aliveThreadId, NULL, aliveThreadReply, NULL) != 0) {
        perror("Error creating aliveThread");
        exit(EXIT_FAILURE);
    }

    // Now, spawn an NM thread, recv NM requests
    // here. Don't wait for it to join. Process 
    // the queries here.
    pthread_t nmThreadId;
    if (pthread_create(&nmThreadId, NULL, nmThread, NULL) != 0) {
        perror("Error creating NM thread");
        exit(EXIT_FAILURE);
    }

    // Now, spawn a client thread, recv client 
    // requests here. Don't wait for it to join. 
    // Wait for a connection request, and process 
    // client request
    pthread_t clientThreadId;
    if (pthread_create(&clientThreadId, NULL, clientThread, NULL) != 0) {
        perror("Error creating client thread");
        exit(EXIT_FAILURE);
    }

    // Wait for user input to exit
    getchar();

    // Close threads and perform cleanup
    pthread_cancel(aliveThreadId);
    pthread_cancel(nmThreadId);
    pthread_cancel(clientThreadId);

    return 0;
}
