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
#define MAX_PATH_LEN 256 
#define MAX_PATHS 1000
#define MAX_ACK_EXTRA_INFO 100

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

// Enum for Request type
typedef enum {
    CREATE_DIR = 0,
    CREATE_FILE,
    READ_FILE,
    WRITE_FILE,
    DELETE_FILE,
    DELETE_DIR
} RequestType;

// Enum for error codes
typedef enum {
    SUCCESS = 0,
    NETWORK_ERROR,
    RUNTIME_ERROR,
    INVALID_INPUT_ERROR,
    OTHER = 69
} ErrorCode;

// Enum for ACK bit types
typedef enum {
    SUCCESS_ACK = 0,
    FAILURE_ACK,
    CHECK_ACK,
    STOP_ACK
} AckBit;

// LOGGING = 0 to store in server logs
// LOGGING = 1 to print on stdout and store in server logs
#define LOGGING 1

// NM IP address
#define NM_IP "127.0.0.1"

// Standard Ports for NM
#define NM_CLT_PORT 8080 // NM listens for clients
#define NM_NEW_SRV_PORT 5049 // NM listens for new servers

#endif // CONSTANTS_H
