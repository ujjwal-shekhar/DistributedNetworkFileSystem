// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include "../utils/headers.h"
#include "../utils/logging.h"
#include "../utils/constants.h"
#include "../utils/structs.h"

bool get_file_data_from_ss(int* clt_srv_fd);

bool send_file_data_to_ss(int* clt_srv_fd, const char* filePath);

bool receiveFileInformation(int* serverSocket);

#endif