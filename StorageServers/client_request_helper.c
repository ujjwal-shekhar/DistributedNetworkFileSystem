#include "server.h"
#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

/**
 * @brief Acquire readlock to allow concurrent file reading.
 * 
 * @param rw_lock: Pointer to the rwlock structure.
 */
void acquire_readlock(rwlock* rw_lock) {
    sem_wait(&rw_lock->lock);
    rw_lock->readers++;
    if (rw_lock->readers == 1) {
        sem_wait(&rw_lock->writeLock);
    }
    sem_post(&rw_lock->lock);
}

/**
 * @brief Release readlock.
 * 
 * @param rw_lock: Pointer to the rwlock structure.
 */
void release_readlock(rwlock* rw_lock) {
    sem_wait(&rw_lock->lock);
    rw_lock->readers--;
    if (rw_lock->readers == 0) {
        sem_post(&rw_lock->writeLock);
    }
    sem_post(&rw_lock->lock);
}

/**
 * @brief Acquire writelock to allow exclusive file writing.
 * 
 * @param rw_lock: Pointer to the rwlock structure.
 */
void acquire_writelock(rwlock* rw_lock) {
    sem_wait(&rw_lock->writeLock);
}

/**
 * @brief Release writelock.
 * 
 * @param rw_lock: Pointer to the rwlock structure.
 */
void release_writelock(rwlock* rw_lock) {
    sem_post(&rw_lock->writeLock);
}

/**
 * @brief Read file present in ss and send it to client
 *
 * @param path : path of the file to be read
 * @param cltSocket : client socket to be used for communication
 *
 * @returns
 */
bool read_file_in_ss(char *path, int *cltSocket) {
        FILE *file = fopen(path, "r");
        if (file == NULL) {
                perror("Error opening file for reading");
                return false;
        }

        FilePacket packet;
        size_t bytesRead;

        printf("Before entering the loop\n");

        char buffer[MAX_CHUNK_SIZE + 1];

        do {
            // memset(0, buffer, MAX_CHUNK_SIZE + 1);

            bytesRead = fread(buffer, 1, MAX_CHUNK_SIZE, file);

            // Fill the remaining part of the buffer with NULL
            buffer[bytesRead] = '\0';
            // memset(buffer + bytesRead, '\0', MAX_CHUNK_SIZE - bytesRead);
            strcpy(packet.chunk, buffer);

            // printf("%zu read bytes: ", bytesRead);
            packet.lastChunk = (feof(file) != 0);

            if (send(*cltSocket, &packet, sizeof(FilePacket), 0) < 0) {
                perror("Error sending file packet to client");
                fclose(file);
                return false;
            }

            printf("Sending this: %s\n", packet.chunk);

            if (packet.lastChunk) {
                printf("This was the last chunk being sent\n");
                break; // No need to continue if it's the last chunk
            }
        } while (bytesRead > 0);

        printf("Done sending\n");

        fclose(file);

        return true;
}

/**
 * @brief Write file present in ss and send ack to client
 *
 * @param path : path of the file to be written to
 * @param cltSocket : client socket to be used for communication
 *
 * @returns
 */
bool write_file_in_ss(char *path, int *cltSocket) {
        FILE *file = fopen(path, "r");
        if (file == NULL) {
                perror("Error opening file for reading");
                return false;
        }

        FilePacket packet;
        size_t bytesRead;

        printf("Before entering the loop for write request\n");

        file = fopen(path, "w");
        if (file == NULL) {
                perror("Error opening file for writing");
                return false;
        }

        fclose(file);

        file = fopen(path, "a");
        printf("the path here is : %s\n", path);

        char buffer[MAX_CHUNK_SIZE + 1];

        // Keep receiving packets until the last packet is received
        do {
            if (recv(*cltSocket, &packet, sizeof(FilePacket), 0) < 0) {
                perror("Error sending file packet to client");
                fclose(file);
                return false;
            }

            printf("Receiving this: %s\n", packet.chunk);

            // Append to the file // GPT complete this ples
            fwrite(packet.chunk, 1, strlen(packet.chunk), file);

            if (packet.lastChunk) {
                break; // No need to continue if it's the last chunk
            }
        } while (bytesRead > 0);

        printf("Done receiving\n");

        fclose(file);

        return true;
}

/**
 * @brief Get file/folder information for a given relative path and send it over a socket.
 * 
 * @param path: The relative path of the file/folder.
 * @param clientSocket: The client socket for sending the information.
 * 
 * @return true if information retrieval and sending are successful, false otherwise.
 */
bool sendFileInformation (const char *path, int* clientSocket) {
    struct stat fileInfo;
    if (stat(path, &fileInfo) != 0) {
        perror("Error getting file information");
        return false;
    }

    char buffer[MAX_CHUNK_SIZE + 1];
    memset(buffer, 0, MAX_CHUNK_SIZE + 1);

    // Build the file information string
    snprintf(buffer, MAX_CHUNK_SIZE,
             "Size: %lld bytes\n"
             "Permissions: %o\n"
             "Last access time: %s"
             "Last modification time: %s",
             (long long)fileInfo.st_size, fileInfo.st_mode & 0777,
             ctime(&fileInfo.st_atime), ctime(&fileInfo.st_mtime));

    // Send the information over the socket
    if (send(*clientSocket, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending file information to client");
        return false;
    }

    return true;
}

/**
 * @brief Create a directory at the specified path.
 * 
 * @param path: The path of the directory to be created.
 * 
 * @return true if the operation is successful, false otherwise.
 */
bool createDirectory(const char* path) {
    if (mkdir(path, 0777) == 0) {
        printf("Directory created: %s\n", path);
        return true;
    } else {
        perror("Error creating directory");
        return false;
    }
}

/**
 * @brief Create a file at the specified path.
 * 
 * @param path: The path of the file to be created.
 * 
 * @return true if the operation is successful, false otherwise.
 */
bool createFile(const char* path) {
    FILE* file = fopen(path, "w");
    if (file != NULL) {
        fclose(file);
        printf("File created: %s\n", path);
        return true;
    } else {
        perror("Error creating file");
        return false;
    }
}

/**
 * @brief Delete a directory at the specified path.
 * 
 * @param path: The path of the directory to be deleted.
 * 
 * @return true if the operation is successful, false otherwise.
 */
bool deleteDirectory(const char* path) {
    if (rmdir(path) == 0) {
        printf("Directory deleted: %s\n", path);
        return true;
    } else {
        perror("Error deleting directory");
        return false;
    }
}

/**
 * @brief Delete a file at the specified path.
 * 
 * @param path: The path of the file to be deleted.
 * 
 * @return true if the operation is successful, false otherwise.
 */
bool deleteFile(const char* path) {
    if (remove(path) == 0) {
        printf("File deleted: %s\n", path);
        return true;
    } else {
        perror("Error deleting file");
        return false;
    }
}

/**
 * @brief List files and empty folders in the given directory recursively.
 * 
 * @param path : Path of the directory to list.
 * @param serverDetails : Pointer to the ServerDetails struct.
 */
void listFilesAndEmptyFolders(const char *path, ServerDetails *serverDetails) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;

    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char fullPath[MAX_PATH_LEN];
            snprintf(fullPath, MAX_PATH_LEN, "%s/%s", path, entry->d_name);

            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                if (stat(fullPath, &fileStat) == -1) {
                    perror("Error in getting file status");
                    continue;
                }

                if (S_ISDIR(fileStat.st_mode)) {
                    // Check if the directory is empty
                    DIR *subDir = opendir(fullPath);
                    struct dirent *subEntry;
                    int isEmpty = 1;

                    if (subDir != NULL) {
                        while ((subEntry = readdir(subDir)) != NULL) {
                            if (strcmp(subEntry->d_name, ".") != 0 && strcmp(subEntry->d_name, "..") != 0) {
                                isEmpty = 0;
                                break;
                            }
                        }
                        closedir(subDir);
                    }

                    if (isEmpty) {
                        // Add a "/" at the end of the path
                        char modifiedPath[MAX_PATH_LEN];
                        snprintf(modifiedPath, sizeof(modifiedPath), "%s/", fullPath + 1);

                        // Copy the modified path to accessible_paths
                        strcpy(serverDetails->accessible_paths[serverDetails->num_paths], modifiedPath);
                        serverDetails->num_paths++;
                    } else {
                        listFilesAndEmptyFolders(fullPath, serverDetails); // Recursive call for non-empty directories
                    }
                } else {
                //     printf("File: %s\n", fullPath + 1);
                    strcpy(serverDetails->accessible_paths[serverDetails->num_paths], 
                               fullPath + 1);
                        serverDetails->num_paths++;
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error in opening directory");
        exit(EXIT_FAILURE);
    }
}
