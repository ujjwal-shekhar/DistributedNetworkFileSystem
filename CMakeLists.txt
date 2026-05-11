cmake_minimum_required(VERSION 3.28)
project(DistributedNetworkFileSystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

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
        src/naming-server/service/nm_load_balancer.cppm
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
    PRIVATE 
        src/client/client_impl.cpp
        src/client/client_utils_impl.cpp
    PUBLIC FILE_SET CXX_MODULES FILES 
        src/client/client.cppm
        src/client/client_types.cppm
        src/client/client_utils.cppm
        src/client/client_prompt.cppm
        src/client/client_warp.cppm
        src/client/client_peek.cppm
        src/client/client_pastevents.cppm
        src/client/client_input.cppm
)
target_link_libraries(client_mod PUBLIC commands_mod network_mod naming_mod logger_mod)

# 8. Job Server Module
add_library(job_mod STATIC)
target_sources(job_mod
    PRIVATE src/job-server/job_server_impl.cpp
    PUBLIC FILE_SET CXX_MODULES FILES src/job-server/job_server.cppm
)
target_link_libraries(job_mod PUBLIC commands_mod network_mod logger_mod client_mod)

# Executables
add_executable(nm src/nm.cpp)
target_link_libraries(nm PRIVATE naming_mod logger_mod commands_mod)

add_executable(clt src/client.cpp)
target_link_libraries(clt PRIVATE client_mod logger_mod commands_mod)

add_executable(ss src/storage_server.cpp)
target_link_libraries(ss PRIVATE storage_mod logger_mod commands_mod)

add_executable(js src/job_server.cpp)
target_link_libraries(js PRIVATE job_mod logger_mod commands_mod)

# Tests
include(GoogleTest)

add_executable(commands_test tests/unit/commands_test.cpp)
target_link_libraries(commands_test PRIVATE commands_mod GTest::gtest_main)
gtest_discover_tests(commands_test)

add_executable(naming_trie_test tests/unit/naming_trie_test.cpp)
target_link_libraries(naming_trie_test PRIVATE naming_mod logger_mod commands_mod network_mod GTest::gtest_main)
gtest_discover_tests(naming_trie_test)

add_executable(logger_test tests/unit/logger_test.cpp)
target_link_libraries(logger_test PRIVATE logger_mod GTest::gtest_main)
gtest_discover_tests(logger_test)

add_executable(storage_test tests/unit/storage_test.cpp)
target_link_libraries(storage_test PRIVATE storage_mod logger_mod commands_mod network_mod GTest::gtest_main)
gtest_discover_tests(storage_test)

add_executable(network_test tests/unit/network_test.cpp)
target_link_libraries(network_test PRIVATE network_mod logger_mod GTest::gtest_main)
gtest_discover_tests(network_test)

add_executable(client_test tests/unit/client_test.cpp)
target_link_libraries(client_test PRIVATE client_mod naming_mod commands_mod network_mod logger_mod GTest::gtest_main)
gtest_discover_tests(client_test)

add_executable(job_server_test tests/unit/job_server_test.cpp)
target_link_libraries(job_server_test PRIVATE client_mod commands_mod GTest::gtest_main)
gtest_discover_tests(job_server_test)
