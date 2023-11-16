#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

void listFilesAndEmptyFolders(const char *path) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;

    if ((dir = opendir(path)) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            char fullPath[1024]; // Adjust the size according to your needs
            snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

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
                        printf("Empty folder: %s\n", fullPath+1);
                    } else {
                        listFilesAndEmptyFolders(fullPath); // Recursive call for non-empty directories
                    }
                } else {
                    printf("File: %s\n", fullPath+1);
                }
            }
        }
        closedir(dir);
    } else {
        perror("Error in opening directory");
        exit(EXIT_FAILURE);
    }
}

int main() {
    const char *directoryPath = "."; // Change this to the desired directory path
    printf("Files in directory '%s' and terminating folders ending with '/':\n", directoryPath);
    listFilesAndEmptyFolders(directoryPath);
    return 0;
}
