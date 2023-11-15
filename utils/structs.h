// structs.h

#ifndef STRUCTS_H
#define STRUCTS_H

// Include necessary headers
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
    int clientSocket;
    int clientPort;
    char clientIP[IP_LEN];
} ClientDetails;

/**
 * @brief ClientRequest struct to store client request
 * 
 * @param clientDetails : client details
 * @param requestType : request type
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
 */
typedef struct ServerDetails {
    int serverID;
} ServerDetails;

/**
 * @brief ackPacket struct to send details
 * 
 * @param errorCode : error code
 * @param ack : ack bit
 *
 */
typedef struct ackPacket {
    ErrorCode errorCode;
    AckBit ack;
} ackPacket;

#endif // STRUCTS_H
