// server.h

#ifndef SERVER_H
#define SERVER_H

#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

// Reader write lock helper functions
void acquire_readlock(rwlock* rw_lock);
void release_readlock(rwlock* rw_lock);
void acquire_writelock(rwlock* rw_lock);
void release_writelock(rwlock* rw_lock);

// Read and write calls from the user interface
bool read_file_in_ss(char *path, int *cltSocket);
bool write_file_in_ss(char *path, int *cltSocket);
bool sendFileInformation(const char *path, int* clientSocket);

void listFilesAndEmptyFolders(const char *path, ServerDetails *serverDetails);

// Helper function to create a directory
bool createDirectory(const char* path);

// Helper function to create a file
bool createFile(const char* path);

// Helper function to delete a directory
bool deleteDirectory(const char* path);

// Helper function to delete a file
bool deleteFile(const char* path);

#endif