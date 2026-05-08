# Modern Distributed Network File System (mDNFS)

A high-performance, modular, and redundant distributed file system implemented using **Modern C++ (C++20/23/26)**.

## Key Features
- **Modern Architecture:** Strictly organized into 7 distinct C++20 modules (`client`, `naming-server`, `storage-server`, `network`, `commands`, `logger`, and `core`).
- **High Performance Networking:** Robust POD-based binary protocol optimized for zero-overhead transmission.
- **Redundancy & Replication:** Automatic file and directory replication across multiple storage servers.
- **Dynamic Scaling:** Storage servers use ephemeral ports and receive dynamic IDs from the Naming Server upon registration.
- **Fault Tolerance:** Real-time heartbeat monitoring detects server failures and automatically redirects clients to online replicas.
- **Modern Concurrency:** Leverages `std::jthread`, `std::stop_token`, and condition variables for safe, efficient multi-threading.
- **Error Handling:** Clean, functional-style error propagation using `std::expected`.

---

## Technical Stack
- **Standard:** C++26 (using `g++-16`)
- **Build System:** CMake 3.28+ with Ninja
- **Language Features:** 
  - C++20 Modules & Partitions
  - Static Reflection (P2996 style)
  - `std::expected` for error handling
  - `std::println` & `std::source_location` for advanced logging
  - RAII-based Socket & Thread management

---

## Getting Started

### Prerequisites
- GCC 16+ (or any compiler with full C++20 modules support)
- CMake 3.28+
- Ninja build system

### Building
```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

---

## Component Usage

### 1. Naming Server (NM)
The central orchestrator that manages the file trie and tracks storage servers. It waits for a minimum number of servers before accepting clients.
```bash
./main_nm [min_ss] [replication_factor]
```
- `min_ss`: Minimum storage servers required to start (default: 3).
- `replication_factor`: How many servers to copy files to (default: 3).

### 2. Storage Server (SS)
The data nodes that store the actual files. Run multiple instances in separate directories for redundancy.
```bash
./main_server [storage_root] [nm_ip] [nm_port]
```
- `storage_root`: Local directory to use for storage (e.g., `SS1`).
- `nm_ip`: IP of the Naming Server (default: 127.0.0.1).
- `nm_port`: Client port of the Naming Server (default: 8080).

### 3. Client
The interactive CLI for performing file operations.
```bash
./main_client
```
Available commands:
- `CREATE_FILE <path>`: Create a replicated file.
- `CREATE_DIR <path>`: Create a replicated directory.
- `READ_FILE <path>`: Read file content (transparently redirected to an online replica).
- `WRITE_FILE <path> <data>`: Update file content.
- `LIST_ALL`: View the merged global directory structure.

---

## Architecture Design
The system is built on a **Modular Micro-Kernel** approach:
1. **Core:** Bundles standard library headers for optimized module compilation.
2. **Commands:** Shared POD structures and protocol definitions.
3. **Network:** RAII wrapper over POSIX Sockets, abstracting FD management.
4. **Logger:** Location-aware logging using C++20 source location.
5. **Naming Server:** Thread-safe Trie and LRU cache for path resolution.
6. **Storage Server:** Fine-grained path-based locking and filesystem abstraction.
7. **Client:** High-level interactive shell for user operations.

---

## Original Team
- Anika Roy
- Prakul Agarwal
- Ujjwal Shekhar

## AI Collaboration
This project was modernized and refactored using **Gemini CLI**, an interactive software engineering agent, focusing on C++26 standards and modular system design.
