// structs.h

#ifndef STRUCTS_H
#define STRUCTS_H

// Include necessary headers
#include "constants.h"

// Define your structures here
struct Client {
    int clientID;
    char name[50];
    // Add more fields as needed
};

struct Server {
    int serverID;
    char name[50];
    // Add more fields as needed
};

#endif // STRUCTS_H
