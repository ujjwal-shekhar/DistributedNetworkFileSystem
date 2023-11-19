// logging.h
#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

// Custom color macros
#define REDCOLOR(text) "\e[0;31m" text "\e[0m"
#define GREENCOLOR(text) "\e[0;32m" text "\e[0m"

// Check if logging is enabled
#if LOGGING == 1

// Define log function for enabled logging
#define LOG(message, plus) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: %s\n", ((plus) ? ("+") : ("-")), message); \
        fclose(fptr); \
    } \
    if (plus) { \
        printf("[%s\e[0m]: %s\n", GREENCOLOR("+"), message); \
    } else { \
        fprintf(stderr, "[%s]: ", REDCOLOR("-")); \
        perror(message); \
    } \
} while (0)

// Define log function for logging server details
#define LOG_SERVER_DETAILS(server) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: Server ID: %d, Server port_nm: %d, Server port_client: %d, Server online: %s\n", \
            GREENCOLOR("+"), server->serverID, server->port_nm, server->port_client, (server->online ? GREENCOLOR("online") : REDCOLOR("offline"))); \
        fclose(fptr); \
    } \
    printf("[%s\e[0m]: Server ID: %d\n    Server port_nm: %d\n    Server port_client: %d\n    Server online: %s\n", \
        GREENCOLOR("+"), server->serverID, server->port_nm, server->port_client, (server->online ? GREENCOLOR("online") : REDCOLOR("offline"))); \
} while (0)

// Define log function for logging client request
#define LOG_CLIENT_REQUEST(clientRequest) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: Client ID: %d, Request Type: %d, Num Args: %d, Arg1: %s, Arg2: %s\n", \
            GREENCOLOR("+"), clientRequest->clientDetails.clientID, clientRequest->requestType, \
            clientRequest->num_args, clientRequest->arg1, clientRequest->arg2); \
        fclose(fptr); \
    } \
    printf("[%s\e[0m]: Client ID: %d\n    Request Type: %d\n      Num Args: %d\n      Arg1: %s\n      Arg2: %s\n", \
        GREENCOLOR("+"), clientRequest->clientDetails.clientID, clientRequest->requestType, \
        clientRequest->num_args, clientRequest->arg1, clientRequest->arg2); \
} while (0)

#else

// Define log function for disabled logging
#define LOG(message, plus) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: %s\n", ((plus) ? ("+") : ("-")), message); \
        fclose(fptr); \
    } \
    if (!plus) { \
        fprintf(stderr, "[%s]: ", REDCOLOR("-")); \
        perror(message); \
    } \
} while (0)

// Define log function for logging server details when logging is disabled
#define LOG_SERVER_DETAILS(server) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: Server ID: %d, Server port_nm: %d, Server port_client: %d, Server online: %s\n", \
            REDCOLOR("-"), server->serverID, server->port_nm, server->port_client, (server->online ? GREENCOLOR("online") : REDCOLOR("offline"))); \
        fclose(fptr); \
    } \
} while (0)

// Define log function for logging client request when logging is disabled
#define LOG_CLIENT_REQUEST(clientRequest) do {} while (0)

#endif

#endif // LOGGING_H
