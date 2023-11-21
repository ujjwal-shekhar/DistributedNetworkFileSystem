// structs.h

#ifndef STRUCTS_H
#define STRUCTS_H

// Include necessary headers
#include "headers.h"
#include "constants.h"

/**
 * @brief ClientDetails struct to store client details
 * 
 * @param clientID : client ID (unique)
 * @param clientSocket : client socket
 * @param clientPort : client port
 * @param clientIP : client IP address
 * 
 */
typedef struct ClientDetails {
    int clientID;
} ClientDetails;

/**
 * @brief ClientRequest struct to store client request
 * 
 * @param clientDetails : client details
 * @param requestType : request type
 * @param num_args : number of arguments
 * @param arg1 : first argument
 * @param arg2 : second argument
 * 
 */
typedef struct ClientRequest {
    ClientDetails clientDetails;
    RequestType requestType;
    int num_args;
    char arg1[MAX_ARG_LEN];
    char arg2[MAX_ARG_LEN];
} ClientRequest;

/**
 * @brief ServerDetails struct to store server details
 * 
 * @param serverID : server ID (unique)
 * @param serverIP : server IP address
 * @param port_nm : port for communication with Naming Server
 * @param port_client : port for communication with client
 * @param accessible_paths : list of accessible paths for this storage server
 * @param online : whether the server is online or not
 * 
 */
typedef struct ServerDetails {
    int serverID;
    char serverIP[IP_LEN];
    int port_nm;
    int port_client;
    int num_paths;
    char accessible_paths[MAX_PATHS][MAX_PATH_LEN];
    bool online;
} ServerDetails;

/**
 * @brief AckPacket struct to send details
 * 
 * @param errorCode : error code
 * @param ack : ack bit
 *
 */
typedef struct AckPacket {
    ErrorCode errorCode;
    AckBit ack;
    int extraInfo[MAX_ACK_EXTRA_INFO];
} AckPacket;

/**
 * @brief FilePacket struct to send details
 * 
 * @param chunk : informatino to send
 * @param lastChunk : True, if this is the last chunk. Stop transmitting data.
 *
 */
typedef struct FilePacket {
    char chunk[MAX_CHUNK_SIZE + 1];
    bool lastChunk;
} FilePacket;

/**
 * @brief Structure representing a node in a trie data structure.
 * 
 * @param children: Array of pointers to child nodes, each corresponding to a character.
 * @param storage_server: Storage server ID for the node.
 * @param isFile: Boolean indicating whether the node represents a file.
 * @param isEndOfWord: Boolean indicating whether the node marks the end of a word (path).
 * 
 */
typedef struct trienode{
    struct trienode *children[NUM_CHARS];
    int storage_server;
    bool isFile;
    bool isEndOfWord;
} trienode;

/**
 * @brief LRU caching for storing recent requests.
 * 
 * @param pathHash: Hash of the accessed path.
 * @param serverID: Storage server ID.
 * @param rank: Rank of the request.
 * 
 */
typedef struct LRU {
    unsigned long long pathHash;  ///< Hash of the accessed path.
    int serverID;                 ///< Storage server ID.
    int rank;                     ///< Rank of athe 
} LRU;

/**
 * @brief Reader-Writer lock to allow concurrent file reading. But only 
 * concurrent file writing is not allowed.
 * 
 * @param readers: Number of readers currently reading the file.
 * @param lock: Binary semaphore
 * @param writeLock: Semaphore to allow only one writer.
 */
typedef struct rwlock {
    int readers;
    sem_t lock;
    sem_t writeLock;
} rwlock;

/**
 * @brief FilePacket struct to send details
 *
 * @param chunk : informatino to send
 * @param lastChunk : True, if this is the last chunk. Stop transmitting data.
 *
 */
typedef struct RedundantServerInfo {
    int active[MAX_REDUN];
    int inactive[MAX_SERVERS];
    int num_active;
    int num_inactive;
} RedundantServerInfo;

#endif // STRUCTS_H
