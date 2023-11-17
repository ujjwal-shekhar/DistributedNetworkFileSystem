// server.c
#include "../utils/headers.h"

#define MY_SRV_IP "127.0.0.1"
ServerDetails serverDetails;

void* aliveThreadReply(void* arg) {
    // Placeholder implementation for aliveThread
    while (1) {
        // Receive "CHECK" packet from the server
        // Respond with "CHECK" packet + serverID
        // Sleep for 10 seconds
        // sleep(10);
        break;
    }
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

    // Accept a connection request
    int nmSocket = accept(sock_fd, (struct sockaddr*) &nm_addr, &nm_addr_len);
    if (nmSocket < 0) {
        perror("Error accepting connection");
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Receive clientRequest
        ClientRequest clientRequest;
        if (recv(nmSocket, &clientRequest, sizeof(ClientRequest), 0) < 0) {
            perror("Error receiving client request");
            exit(EXIT_FAILURE);
        }

        // Process clientRequest
        /* TBD */

        // Send SUCCESS ACK to NM
        ackPacket nmAck;
        nmAck.errorCode = SUCCESS;
        nmAck.ack = SUCCESS_ACK;
        if (send(nmSocket, &nmAck, sizeof(ackPacket), 0) < 0) {
            perror("Error sending ack packet to NM");
            exit(EXIT_FAILURE);
        }

        // Close the socket
        close(nmSocket);
    }
    return NULL;
}

void* clientThread(void* arg) {
    // Create a socket
    int sock_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    printf("Started listening on port1 %d\n", serverDetails.port_client);

    // Create a sockaddr_in struct for the server
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SOCKET_FAMILY;
    server_addr.sin_port = htons(serverDetails.port_client);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    printf("Started listening on port2 %d\n", serverDetails.port_client);
    // Bind the socket to the port
    if (bind(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }
    printf("Started listening on port3 %d\n", serverDetails.port_client);

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

        // Print the response type
        if (clientRequest.requestType == READ_FILE) {
            printf("Read file: %s\n", clientRequest.arg1);
        } else if (clientRequest.requestType == WRITE_FILE) {
            printf("Write file: %s\n", clientRequest.arg1);
        } else if (clientRequest.requestType == GET_FILE_INFO) {
            printf("Get file info of : %s\n", clientRequest.arg1);
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <serverID> <CLT_PORT> <NM_PORT>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Make a ServerDetails with the given serverID
    serverDetails.serverID = atoi(argv[1]);
    strcpy(serverDetails.serverIP, MY_SRV_IP); // Replace with your actual IP
    serverDetails.port_client = atoi(argv[2]);
    serverDetails.port_nm = atoi(argv[3]);
    serverDetails.online = true;

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

    // Connect to the server
    if (connect(sock_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        printf("Error connecting to server\n");
        exit(-1);
    }

    // Add accessible paths later
    /* TBD */

    // Send the server details to the NM
    if (send(sock_fd, &serverDetails, sizeof(ServerDetails), 0) < 0) {
        perror("Error sending server details to NM");
        exit(EXIT_FAILURE);
    }

    // The NM sends back an ack packet
    // Get the server ID assigned by NM to you
    // Store this server ID here
    ackPacket nmAck;
    
    // Receive the ack packet
    if (recv(sock_fd, &nmAck, sizeof(ackPacket), 0) < 0) {
        perror("Error receiving ack packet");
        exit(EXIT_FAILURE);
    }

    // If this is a FAILURE_ACK, just die
    if (nmAck.ack == FAILURE_ACK) {
        printf("Error: NM returned FAILURE_ACK\n");
        exit(EXIT_FAILURE);
    }

    printf("NM returned\n");

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
