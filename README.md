# Modern Distributed Network File System (mDNFS)

A high-performance, modular, and redundant distributed file system implemented using **Modern C++ (C++20/23/26)**.

## Key Features

- **Modern Architecture:** Strictly organized into 6 distinct C++20 modules (`client`, `naming-server`, `storage-server`, `network`, `commands`, and `logger`).
- **High Performance Networking:** Robust POD-based binary protocol with full hostname resolution support for distributed environments (Docker-ready).
- **Dynamic Redundancy (Auto-Healing):** Naming Server automatically detects node failures and orchestrates SS-to-SS re-replication to maintain the target replication factor.
- **Full Synchronization:** `WRITE_FILE` operations are automatically synchronized across all online replicas.
- **Fault Tolerance & Chaos Resistance:** Automatic load balancing for `READ_FILE` and robust connection retry logic across all components.
- **Recursive Operations:** Full support for `DELETE_DIR` with automatic descendant cleanup in both the trie and physical storage.
- **Isolated Storage:** Storage servers automatically isolate data into unique `SS_<ID>` directories based on Naming Server assignment.
- **Modern Concurrency:** Leverages `std::jthread`, `std::stop_token`, and fine-grained path-based locking for safe, high-concurrency access.
- **Error Handling:** Clean, functional-style error propagation using `std::expected` and detailed distributed diagnostics.

---

## Technical Stack

- **Standard:** C++26 (using `g++-16`)
- **Build System:** CMake 3.30+ (for C++26 support) with Ninja
- **Testing:** Google Test (GTest) 1.14+
- **Virtualization:** Docker & Docker Compose for cluster simulation
- **Language Features:** 
  - C++20 Modules & Partitions
  - Static Reflection (P2996 style)
  - `std::expected` for error handling
  - `std::println`, `std::format` & `std::source_location` for UTC-timestamped logging
  - RAII-based Socket & Thread management

---

## Getting Started

### Prerequisites

- GCC 16+ (or any compiler with full C++20 modules support)
- CMake 3.30+
- Ninja build system
- Docker & Docker Compose (for simulation tests)

### Building
```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

---

## Testing

### Unit Tests
Logical correctness of individual modules is verified using an extensive Google Test suite (14+ tests).
```bash
cd build
ctest --output-on-failure
```

### Integration, Chaos & Auto-Healing Tests
End-to-end behavior and node-failure resilience are verified using Dockerized cluster simulations.
```bash
./tests/run_integration_tests.sh
./tests/run_chaos_tests.sh
./tests/run_auto_heal_test.sh
```

---

## Component Usage

### 1. Naming Server (NM)
The central orchestrator that manages the file trie and tracks storage servers.
```bash
./nm [min_ss] [replication_factor]
```
- `min_ss`: Minimum storage servers required to start (default: 3).
- `replication_factor`: Target copies for every file (default: 3).

### 2. Storage Server (SS)
Data nodes that store the files. They automatically create isolated storage roots.
```bash
./ss [storage_root] [nm_host] [nm_port]
```
- `storage_root`: (Optional) Custom root directory. Defaults to `SS_<ID>`.
- `nm_host`: Hostname/IP of the Naming Server (default: 127.0.0.1).

### 3. Client (CLT)
Interactive CLI for performing file operations.
```bash
./clt [nm_host] [nm_port]
```
Available commands:
- `CREATE_FILE <path>`: Create a replicated file.
- `CREATE_DIR <path>`: Create a replicated directory.
- `READ_FILE <path>`: Read file content (load-balanced across replicas).
- `WRITE_FILE <path> [local_path]`: 
  - Provide `local_path` to copy a file from your machine to the DFS.
  - Omit `local_path` to type content in the terminal (end with `END` on a new line).
- `LIST_FILES` or `LIST_ALL`: View the merged global directory structure.
- `GET_FILE_INFO <path>`: View replica locations, SS IDs, and network details.
- `DELETE_FILE <path>` / `DELETE_DIR <path>`: Redundantly remove files or entire directory trees.

---

## Architecture Design

The system is built on a **Modular Micro-Kernel** approach:

1. **Commands:** Shared POD structures and protocol definitions (Binary-safe).
2. **Network:** RAII wrapper over POSIX Sockets with hostname resolution and reliable `send_all`/`receive_all` primitives.
3. **Logger:** Location-aware logging with UTC timestamps and source location.
4. **Naming Server:** Thread-safe Trie and LRU cache for path resolution and cluster orchestration (Auto-healing controller).
5. **Storage Server:** Fine-grained path-based locking and automatic filesystem isolation.
6. **Client:** High-level interactive shell with connection retry logic.

---

## AI Collaboration

This project was modernized, refactored, and tested using **Gemini CLI**, an interactive software engineering agent, focusing on C++26 standards, distributed robustness, and automated resilience testing.
