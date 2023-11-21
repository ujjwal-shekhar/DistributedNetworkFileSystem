#include "nm.h"

#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

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
 * 
 * @return false on error, true on success
 */
bool sendAckToClient(int* clientSocket, AckPacket* ack) {
    if (send(*clientSocket, ack, sizeof(AckPacket), 0) < 0) {
        LOG("Error sending ACK", false);
        return false;
    }
    return true;
}

/**
 * @brief Sends server details to the client.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param serverDetails : Pointer to ServerDetails struct containing server details.
 * 
 * @return true if server details was sent, false otherwise.
 */
bool sendServerDetailsToClient(int* clientSocket, ServerDetails* serverDetails) {
    printf("%d is the client socket\n", *clientSocket);
    if (send(*clientSocket, serverDetails, sizeof(ServerDetails), 0) < 0) {
        LOG("Error sending server details to client", false);
        return false;
    }
    return true;
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
bool sendConnectionAcknowledgment(int* clientSocket, AckBit ackType, ErrorCode errorCode) {
    LOG("Sending a connection acknowledgement", true);
    AckPacket cltAck;
    cltAck.ack = ackType;
    cltAck.errorCode = errorCode;
    return sendAckToClient(clientSocket, &cltAck);
}

/**
 * @brief Forwards a client request to the storage server.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param clientRequest : Pointer to ClientRequest struct containing client request details.
 * @param ss_num : Storage server number.
 * @param servers : Pointer to an array of ServerDetails structs containing server details.
 */
bool forwardClientRequestToServer(int* clientSocket, ClientRequest* clientRequest, int ss_num, ServerDetails* servers, trienode* root) {
    // Connect to the storage server
    int storage_fd; connectToStorageServer(&storage_fd, ss_num, servers);
    if (storage_fd < 0) {
        return false;
    }

    // Send the clientRequest to the storage server
    if (send(storage_fd, clientRequest, sizeof(ClientRequest), 0) < 0) {
        LOG("Can't send clientRequest to storage server", false);
        close(storage_fd);
        return false;
    }
    LOG("Sent ClientRequest to storage server successfully", true);

    // Receive the acknowledgment from the storage server
    AckPacket nmAck;
    if (recv(storage_fd, &nmAck, sizeof(AckPacket), 0) < 0) {
        LOG("Error receiving acknowledgment from storage server", false);
        close(storage_fd);
        return false;
    }
    LOG("Received acknowledgement from storage server", true);

    // Receive the new server details from the storage server
    ServerDetails newServerDetails;
    if (recv(storage_fd, &newServerDetails, sizeof(newServerDetails), 0) < 0) {
        LOG("Error receiving new server details from storage server", false);
        close(storage_fd);
        return false;
    }

    // Update the server details in the servers[ss_num]
    // call trieinsert on every path in the list
    // This will handle trie update for new paths
    servers[ss_num].num_paths = newServerDetails.num_paths;

    // Overwrite the list of accessible paths with what was just obtained
    for (int i = 0; i < newServerDetails.num_paths; i++) {
        strcpy(servers[ss_num].accessible_paths[i], newServerDetails.accessible_paths[i]);
        trieinsert(&root, servers[ss_num].accessible_paths[i], ss_num);
    }

    /* @Anika-Roy please implement trie update */

    // Forward the acknowledgment to the client
    if (!sendAckToClient(clientSocket, &nmAck)) {
        return false;
    }

    LOG("Forwarded acknowledgement from storage server to client", true);

    close(storage_fd);

    return true;
}

/**
 * @brief Connects to the storage server.
 * 
 * @param storage_fd : Socket to connect to storage server
 * @param ss_num : Storage server number.
 * @param servers : Pointer to an array of ServerDetails structs containing server details.
 * 
 * @return Storage server socket file descriptor.
 */
bool connectToStorageServer(int* storage_fd, int ss_num, ServerDetails* servers) {
    *storage_fd = socket(SOCKET_FAMILY, SOCKET_TYPE, SOCKET_PROTOCOL);
    if (*storage_fd < 0) {
        LOG("Error creating storage socket", false);
        return false;
    }

    LOG("Storage server socket for NM connection successfully made", true);

    // Create a sockaddr_in struct for the storage socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = SOCKET_FAMILY;
    server_addr.sin_port = htons(servers[ss_num].port_nm);
    server_addr.sin_addr.s_addr = inet_addr(servers[ss_num].serverIP);

    // Connect to the storage server
    if (connect(*storage_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG("Can't connect to storage server", true);
        close(*storage_fd);
        return false;
    }

    LOG("NM connected to storage server", true);

    return true;
}

/**
 * @brief Handles a client request.
 * 
 * @param clientSocket : Client socket file descriptor.
 * @param clientRequest : Pointer to ClientRequest struct containing client request details.
 * @param ss_num : Storage server number.
 * @param servers : Pointer to an array of ServerDetails structs containing server details.
 * 
 * @return  true on success, false on failure
 */
bool handleClientRequest(int* clientSocket, ClientRequest* clientRequest, int ss_num, ServerDetails* servers, trienode* root) {
    LOG_CLIENT_REQUEST(clientRequest);

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
                LOG("Request Type : NON-PRIVILEDGED", true);
                if (!sendConnectionAcknowledgment(clientSocket, CNNCT_TO_SRV_ACK, SUCCESS)) {
                    LOG("Connection acknowledgement failed", false);
                    return false;
                } else {
                    LOG("Connection acknowledgement succeeded", true);
                }

                // Send the ServerDetails to the client
                if (!sendServerDetailsToClient(clientSocket, &servers[ss_num])) {
                    return false;
                } else {
                    LOG("Server Details sent to client", true);
                }

                LOG("Forwarding NON-PRIVILEDGED request to client successful", true);
                return true;
            } else {
                LOG("Request Type : PRIVILEDGED", true);
                if (!sendConnectionAcknowledgment(clientSocket, INIT_ACK, SUCCESS)) {
                    LOG("Connection acknowledgement failed", false);
                    return false;
                }

                // Send the clientRequest to the storage server
                if (!forwardClientRequestToServer(clientSocket, clientRequest, ss_num, servers, root)) {
                    LOG("Couldn't forward request to storage server", false);
                    return false;
                }

                LOG("Forwarding PRIVILEDGED request to storage server successful", true);
                return true;
            }
        } else {
            LOG("The storage server was offline", false);
            handleServerOffline(clientSocket);
            return false;
        }
    } else {
        LOG("The path given by the client is invalid", false);
        handleWrongPath(clientSocket);
        return false;
    }
}

/***************************************************/
/*            Efficient Search goes here           */
/***************************************************/

/**
 * @brief To find the storage server idx given the path
 * 
 * @param address The address we are looking for
 * @param servers The list of ServerDetail objects
 * 
 * @returns the server idx
 */
int findStorageServer(char* address, trienode* root, LRU* lru) {
    // Calculate the hash of the address
    unsigned long long hash = 0;
    for (int i = 0; i < strlen(address); i++) {
        hash = (hash * ROLLING_PRIME + (address[i] - 'a')) % ROLLING_MODULO;
    }

    // Decay the rank of each entry
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        lru[i].rank++;
    }
    
    int maxRank = -1;
    int maxRankIdx = -1;
    // Check if the entry is in the LRU
    for (int i = 0; i < MAX_CACHE_SIZE; i++) {
        if (maxRank < lru[i].rank) {
            maxRank = lru[i].rank;
            maxRankIdx = i;
        }

        if (lru[i].pathHash == hash) {
            lru[i].rank = 0;
            return lru[i].serverID;
        }
    }

    // Put the entry in the LRU
    lru[maxRankIdx].pathHash = hash;
    lru[maxRankIdx].rank = 0;
    lru[maxRankIdx].serverID = search_trie(root, address);

   return lru[maxRankIdx].serverID;
}

/**
 * @brief Creates a new trie node and initializes its members.
 * 
 * @return A pointer to the newly created trie node.
 */
trienode* createnode() {
    trienode* node = (trienode*) malloc(sizeof(trienode));

    for(int i=0; i<NUM_CHARS; i++){
        node->children[i] = NULL;
    }
    node->storage_server = -1;
    node->isFile = false;
    node->isEndOfWord = false;
    return node;
}

/**
 * @brief Inserts a path into the trie along with the corresponding storage server ID.
 * 
 * @param root: Pointer to the root of the trie.
 * @param signedtext: Path to be inserted.
 * @param serverID: Storage server ID for the path.
 */
void trieinsert (trienode** root, char* signedtext, int serverID) {
    if (*root == NULL) {
        *root = createnode();
    }

    unsigned char* text = (unsigned char*) signedtext;     // just in case we don't get negative indexes
    trienode* curr = *root;
    int length = strlen(signedtext);

    for (int i = 0 ; i < length ; i++) {
        if(curr->children[text[i]] == NULL){
            // create a node
            curr->children[text[i]] = createnode();
        }

        curr = curr->children[text[i]];
        if (text[i]=='/') {
            curr->isFile = false;
            curr->storage_server = serverID;
            curr->isEndOfWord = true;
        }
    }

    if (text[length-1]!='/') {
        curr->isFile = true;
        curr->isEndOfWord = true;
        curr->storage_server = serverID;
    }
    
}

/**
 * @brief Searches for a path in the trie and returns the corresponding storage server ID.
 * 
 * @param root: Pointer to the root of the trie.
 * @param signedtext: Path to be searched.
 * 
 * @return The storage server ID if the path is found, -1 otherwise.
 */
int search_trie (trienode* root, char* signedtext) {
    if(root == NULL){
        return false;
    }

    printf("Searching for the path : %s", signedtext);

    unsigned char* text = (unsigned char*)signedtext;
    trienode* curr = root;
    int length = strlen(signedtext);

    for(int i=0 ; i<length ; i++){
        if(curr->children[text[i]] == NULL){
            return false;
        }

        curr = curr->children[text[i]];
    }

    return ((curr->isEndOfWord) ? (curr->storage_server) : (-1)); // -1 marks path nowhere
}
