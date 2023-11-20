#include "client.h"

#include "../utils/logging.h"
#include "../utils/headers.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

/**
 * @brief Retrieve file data from the storage server and print it to stdout.
 * 
 * @param clt_srv_fd : Client-Server socket file descriptor.
 * 
 * @return true if the operation is successful, false otherwise.
 */
bool get_file_data_from_ss(int* clt_srv_fd) {
    FilePacket packet;

    while (true) {
        // Receive file data packet from the server
        ssize_t bytesRead = recv(*clt_srv_fd, &packet, sizeof(FilePacket), 0);

        if (bytesRead < 0) {
            perror("Error reading from storage server");
            return false;
        }

        if (bytesRead == 0) {
            // Connection closed by the server
            break;
        }

        // Print the received data to stdout
        // fwrite(packet.chunk, 1, bytesRead, stdout);
        printf("%s", packet.chunk);

        if (packet.lastChunk) {
            // Last chunk received, exit the loop
            break;
        }
    }

    printf("\n");

    return true;
}

/**
 * @brief Send file data to the storage server.
 * 
 * @param clt_srv_fd : Client-Server socket file descriptor.
 * @param filePath : Path of the file to be written.
 * 
 * @return true if the operation is successful, false otherwise.
 */
bool send_file_data_to_ss(int* clt_srv_fd, const char* filePath) {
//     // Open the file for reading
//     FILE* file = fopen(filePath, "rb");
//     if (!file) {
//         perror("Error opening file for reading");
//         return false;
//     }

//     struct FilePacket packet;
//     size_t bytesRead;

//     while ((bytesRead = fread(packet.chunk, 1, MAX_CHUNK_SIZE, file)) > 0) {
//         packet.lastChunk = feof(file) != 0;

//         // Send file data packet to the server
//         if (send(*clt_srv_fd, &packet, sizeof(struct FilePacket), 0) < 0) {
//             perror("Error sending file data to storage server");
//             fclose(file);
//             return false;
//         }

//         if (packet.lastChunk) {
//             break;  // No need to continue if it's the last chunk
//         }
//     }

//     fclose(file);
    return true;

}
