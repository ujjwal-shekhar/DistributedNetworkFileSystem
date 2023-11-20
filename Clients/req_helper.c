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
        FILE *tempFile = fopen("temp_buffer.txt", "w");
        if (tempFile == NULL) {
            perror("Error opening temp_buffer.txt for writing");
            return false;
        }

        printf("Enter text. Press Enter twice to finish:\n");

        char buffer[MAX_CHUNK_SIZE + 1];
        int consecutiveEnters = 0;

        while (true) {
            // Read a line from stdin
            if (fgets(buffer, MAX_CHUNK_SIZE, stdin) == NULL) {
                perror("Error reading from stdin");
                fclose(tempFile);
                return false;
            }

            // Check for consecutive enters
            if (buffer[0] == '\n') {
                consecutiveEnters++;
                if (consecutiveEnters == 2) {
                    break;  // Exit the loop when two consecutive enters are encountered
                }
            } else {
                consecutiveEnters = 0;  // Reset consecutiveEnters when a non-empty line is encountered
            }

            // Write the line to the temporary file
            fputs(buffer, tempFile);
        }

        // Close the temporary file
        fclose(tempFile);

        printf("Text entered and saved to temp_buffer.txt.\n");

        // Open the temporary file for reading
        tempFile = fopen("temp_buffer.txt", "r");
        if (tempFile == NULL) {
            perror("Error opening temp_buffer.txt for reading");
            return false;
        }

        printf("Made the file in client.\n");
        // Send the file data to the server
        FilePacket packet;
        size_t bytesRead;

        while (true) {
            // Read a chunk from the file
            bytesRead = fread(packet.chunk, 1, MAX_CHUNK_SIZE, tempFile);
            packet.chunk[bytesRead] = '\0';

            printf("Made the file in client : %s\n", packet.chunk);
            
            if (ferror(tempFile)) {
                perror("Error reading from temp_buffer.txt");
                fclose(tempFile);
                return false;
            }

            // Set the lastChunk flag
            packet.lastChunk = (feof(tempFile) != 0);

            // Send the chunk to the server
            if (send(*clt_srv_fd, &packet, sizeof(FilePacket), 0) < 0) {
                perror("Error sending file packet to server");
                fclose(tempFile);
                return false;
            }

            if (packet.lastChunk) {
                break; // No need to continue if it's the last chunk
            }
        }

        // Close the temporary file
        fclose(tempFile);

        // Delete the temporary file
        if (remove("temp_buffer.txt") != 0) {
            perror("Error deleting temp_buffer.txt");
            return false;
        }

    return true;
}
