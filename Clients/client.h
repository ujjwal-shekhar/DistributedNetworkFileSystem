// client.h
#ifndef CLIENT_H
#define CLIENT_H

#include "../utils/headers.h"

bool get_file_data_from_ss(int* clt_srv_fd);

bool send_file_data_to_ss(int* clt_srv_fd, const char* filePath);

#endif