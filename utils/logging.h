// logging.h
#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

// Check if logging is enabled
#if LOGGING == 1

// Define log function for enabled logging
#define LOG(message, plus) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: %s\n", ((plus) ? ("+") : ("-")), message); \
        fclose(fptr); \
    } \
    printf("[%s\e[0m]: %s\n", ((plus) ? ("\e[0;32m+") : ("\e[0;31m-")), message); \
} while (0)

#else

// Define log function for disabled logging
#define LOG(message, plus) do { \
    FILE *fptr = fopen(NM_LOG_FILE, "a"); \
    if (fptr) { \
        fprintf(fptr, "[%s]: %s\n", ((plus) ? ("+") : ("-")), message); \
        fclose(fptr); \
    } \
} while (0)

#endif

#endif // LOGGING_H
