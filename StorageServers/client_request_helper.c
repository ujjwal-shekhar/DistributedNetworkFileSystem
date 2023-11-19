#include "server.h"

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
