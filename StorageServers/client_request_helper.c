#include "server.h"
#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

/**
 * @brief
 *
 * @param path
 * @param cltSocket
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

        char buffer[MAX_CHUNK_SIZE];

        while ((bytesRead = fread(buffer, 1, MAX_CHUNK_SIZE, file)) > 0) {
                printf("%d read bytes: ", bytesRead);
                break;
                packet.lastChunk = (feof(file) != 0);

                if (send(*cltSocket, &packet, sizeof(FilePacket), 0) < 0) {
                        return false;
                        perror("Error sending file packet to client");
                }

                printf("\nSending this : %s\n", packet.chunk);

                if (packet.lastChunk) {
                        break; // No need to continue if it's the last chunk
                }
        }

        fclose(file);
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