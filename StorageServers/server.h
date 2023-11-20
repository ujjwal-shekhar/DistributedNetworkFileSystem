// server.h

#ifndef SERVER_H
#define SERVER_H

#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

// Read and write calls from the user interface
bool read_file_in_ss(char *path, int *cltSocket);
bool write_file_in_ss(char *path, int *cltSocket);

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