#include "../utils/headers.h"

/**
 * @brief Check if the user request of the correct format
 * 
 * @param request : the request string
 * @return true if valid syntax
 */
bool isValidRequest (char* request) {
    // Tokenize the string by space
    char* token = strtok(request, " ");

    // Check if the first token is a valid command
    if (
        strcmp(token, CREATEDIR) == 0 || 
        strcmp(token, CREATEFILE) == 0 || 
        strcmp(token, READFILE) == 0 || 
        strcmp(token, WRITEFILE) == 0 || 
        strcmp(token, DELETEFILE) == 0 || 
        strcmp(token, DELETEDIR) == 0
    ) {
        // Check if there is exactly one more token
        token = strtok(NULL, " ");
        if (
            (token != NULL) &&              // There was a token
            (strtok(NULL, " ") == NULL)     // And there wasn't a second token
        ) {

            return true;
        } else {
            return false;
        }
    }
}

int main() {
    // We will ask the user to input their request
    char request[MAX_REQUEST_SIZE + 1];

    // Read the line
    printf("Enter your request: ");
    fgets(request, MAX_REQUEST_SIZE, stdin);

    // Validate the request syntax

    return 0;
}