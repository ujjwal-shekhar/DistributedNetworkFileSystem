cmake_minimum_required(VERSION 3.28)
project(DistributedNetworkFileSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# 2. Commands Module
add_library(commands_mod STATIC)
target_sources(commands_mod PUBLIC FILE_SET CXX_MODULES FILES 
    src/commands/commands.cppm
)
target_link_libraries(commands_mod PUBLIC)

# 3. Logger Module
add_library(logger_mod STATIC)
target_sources(logger_mod 
    PRIVATE src/logger/logger_impl.cpp
    PUBLIC FILE_SET CXX_MODULES FILES src/logger/logger.cppm
)
target_link_libraries(logger_mod PUBLIC)

# 4. Network Module
add_library(network_mod STATIC)
target_sources(network_mod PUBLIC FILE_SET CXX_MODULES FILES 
    src/network/network.cppm
    src/network/network_internal.cppm
    src/network/network_types.cppm
)
target_link_libraries(network_mod PUBLIC)

# 5. Naming Server Module
add_library(naming_mod STATIC)
target_sources(naming_mod 
    PRIVATE 
        src/naming-server/network/nm_orchestrator.cpp
        src/naming-server/service/naming_impl.cpp
    PUBLIC FILE_SET CXX_MODULES FILES 
        src/naming-server/service/naming.cppm
        src/naming-server/service/naming_types.cppm
        src/naming-server/service/naming_internal.cppm
        src/naming-server/service/commands.cppm
        src/naming-server/auth/admin.cppm
        src/naming-server/auth/client_manager.cppm
        src/naming-server/network/server.cppm
)
target_link_libraries(naming_mod PUBLIC commands_mod network_mod logger_mod)

# 6. Storage Server Module
add_library(storage_mod STATIC)
target_sources(storage_mod 
    PRIVATE 
        src/storage-server/storage_impl.cpp
        src/storage-server/filesystem/fs_impl.cpp
    PUBLIC FILE_SET CXX_MODULES FILES 
        src/storage-server/storage.cppm
        src/storage-server/storage_types.cppm
        src/storage-server/storage_sync.cppm
        src/storage-server/filesystem/fs_ops.cppm
)
target_link_libraries(storage_mod PUBLIC commands_mod network_mod logger_mod)

# 7. Client Module
add_library(client_mod STATIC)
target_sources(client_mod 
    PRIVATE src/client/client_impl.cpp
    PUBLIC FILE_SET CXX_MODULES FILES 
        src/client/client.cppm
        src/client/client_types.cppm
)
target_link_libraries(client_mod PUBLIC commands_mod network_mod naming_mod logger_mod)

# Executables
add_executable(nm src/nm.cpp)
target_link_libraries(nm PRIVATE naming_mod logger_mod commands_mod)

add_executable(clt src/client.cpp)
target_link_libraries(clt PRIVATE client_mod logger_mod commands_mod)

add_executable(ss src/storage_server.cpp)
target_link_libraries(ss PRIVATE storage_mod logger_mod commands_mod)
