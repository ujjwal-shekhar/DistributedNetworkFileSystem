// logging.h
#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#ifdef LOGGING
#if LOGGING == 1
#define LOG(message) printf("LOG: %s\n", message)
#else
#define LOG(message)
#endif
#else
#define LOG(message)
#endif

#endif // LOGGING_H
