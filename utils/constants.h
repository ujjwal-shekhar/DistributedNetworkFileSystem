// constants.h

#ifndef CONSTANTS_H
#define CONSTANTS_H

// Define your constants here
#define MAX_CLIENTS 100
#define MAX_SERVERS 10
#define SOCKET_FAMILY AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define SOCKET_PROTOCOL 0
#define IP_LEN 16
#define MAX_REQUEST_SIZE 1024
#define MAX_ARG_LEN 256
#define MAX_LISTEN_BACKLOG 20
#define MAX_PATH_LEN 1024
#define MAX_PATHS 1000
#define MAX_ACK_EXTRA_INFO 100
#define NUM_INIT_SERVERS 4
#define MAX_CHUNK_SIZE 1024
#define NUM_CHARS 256
#define MAX_CACHE_SIZE 5
#define ROLLING_PRIME 31
#define ROLLING_MODULO 1000000007
#define MAX_REDUN 2

// Timeout intervals
#define MAX_NM_TO_CLT_TIMEOUT 30
#define MAX_CLT_TO_NM_TIMEOUT 30
#define MAX_NM_TO_SRV_TIMEOUT 30
#define MAX_SRV_TO_NM_TIMEOUT 30
#define MAX_CLT_TO_SRV_TIMEOUT 30
#define MAX_SRV_TO_CLT_TIMEOUT 30

// Valid commands for the client
#define CREATEDIR "CREATE_DIR"
#define CREATEFILE "CREATE_FILE"
#define READFILE "READ_FILE"
#define WRITEFILE "WRITE_FILE"
#define DELETEFILE "DELETE_FILE"
#define DELETEDIR "DELETE_DIR"
#define GETINFO "GET_INFO"
#define LISTALL "LIST_ALL"

// Enum for Request type
typedef enum {
    /* Priviledged */
    CREATE_DIR = 0,
    CREATE_FILE,
    DELETE_DIR,
    DELETE_FILE,

    /* Non-priviledged */
    GET_FILE_INFO,
    READ_FILE,
    WRITE_FILE,
    LIST_ALL
} RequestType;

// Enum for error codes
typedef enum {
    SUCCESS = 0,
    NETWORK_ERROR,
    RUNTIME_ERROR,
    INVALID_INPUT_ERROR,
    SERVER_ALREADY_REGISTERED,
    SERVER_OFFLINE,
    WRONG_PATH,
    OTHER = 69
} ErrorCode;

// Enum for ACK bit types
typedef enum {
    SUCCESS_ACK = 0,
    FAILURE_ACK,
    CHECK_ACK,
    INIT_ACK,
    CNNCT_TO_SRV_ACK, // Send this to client, to get them ready for server connection
    STOP_ACK
} AckBit;

#define NM_LOG_FILE "./naming_server.log"

// NM IP address
#define NM_IP "127.0.0.1"

// Standard Ports for NM
#define NM_CLT_PORT 8080 // NM listens for clients
#define NM_ALIVE_PORT 5048 // NM listens for alive messages
#define NM_NEW_SRV_PORT 5049 // NM listens for new servers
#define NM_COMM_SRV_PORT 4050 // NM communicates for servers

#endif // CONSTANTS_H
