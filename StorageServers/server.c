#include "../utils/headers.h"

#define MY_SRV_IP "127.0.0.1"
#define MY_SRV_NM_PORT 6060
#define MY_SRV_CLT_PORT 7070

void* aliveThread(void* arg) {
    // Placeholder implementation for aliveThread
    while (1) {
        // Receive "CHECK" packet from the server
        // Respond with "CHECK" packet + serverID
        // Sleep for 10 seconds
        sleep(10);
    }
    return NULL;
}

void* nmThread(void* arg) {
    // Placeholder implementation for NM thread
    while (1) {
        // Receive NM requests here
        // Process the queries
    }
    return NULL;
}

void* clientThread(void* arg) {
    // Placeholder implementation for client thread
    while (1) {
        // Wait for a connection request
        // Process client requests
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <serverID>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Make a ServerDetails with the given serverID
    ServerDetails serverDetails;
    serverDetails.serverID = atoi(argv[1]);
    strcpy(serverDetails.serverIP, MY_SRV_IP); // Replace with your actual IP
    serverDetails.port_nm = MY_SRV_NM_PORT;
    serverDetails.port_client = MY_SRV_CLT_PORT;
    serverDetails.online = false;

    // Add accessible paths later
    /* TBD */

    // The NM sends back an ack packet
    // Get the server ID assigned by NM to you
    // Store this server ID here
    ackPacket nmAck;
    // Assume nmAck is received from NM, update serverDetails.serverID accordingly

    // If this is a FAILURE_ACK, just die
    if (nmAck.ack == FAILURE_ACK) {
        printf("Error: NM returned FAILURE_ACK\n");
        exit(EXIT_FAILURE);
    }

    // // Get the 0th entry of the extraInfo
    // // Now, set the serverID to this 
    // // value
    // serverDetails.serverID = nmAck.extraInfo[0];

    // // Now, spawn an aliveThread. This thread 
    // // receives a "CHECK" packet from
    // // the server, and replies with a "CHECK"
    // // packet + serverID. Sleep for 10 seconds.
    // // Don't wait for this thread to join
    // pthread_t aliveThreadId;
    // if (pthread_create(&aliveThreadId, NULL, aliveThread, NULL) != 0) {
    //     perror("Error creating aliveThread");
    //     exit(EXIT_FAILURE);
    // }

    // // Now, spawn an NM thread, recv NM requests
    // // here. Don't wait for it to join. Process 
    // // the queries here.
    // pthread_t nmThreadId;
    // if (pthread_create(&nmThreadId, NULL, nmThread, NULL) != 0) {
    //     perror("Error creating NM thread");
    //     exit(EXIT_FAILURE);
    // }

    // // Now, spawn a client thread, recv client 
    // // requests here. Don't wait for it to join. 
    // // Wait for a connection request, and process 
    // // client request
    // pthread_t clientThreadId;
    // if (pthread_create(&clientThreadId, NULL, clientThread, NULL) != 0) {
    //     perror("Error creating client thread");
    //     exit(EXIT_FAILURE);
    // }

    // // Wait for user input to exit
    // getchar();

    // // Close threads and perform cleanup (not shown in the placeholder)

    return 0;
}
