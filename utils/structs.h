// structs.h

#ifndef STRUCTS_H
#define STRUCTS_H

// Include necessary headers
#include <stdbool.h>
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

#endif // STRUCTS_H
