# MIGRATION: Modernization of Distributed Network File System

This document tracks the refactoring journey from a legacy C-style implementation to a **Modern C++26** distributed system.

## Phase 1: C-Shell Integration & Modularization

### 1. Module Refactoring
- **Partitioning:** The `client` module was refactored into distinct C++20 partitions:
    - `client:types`: Shared PODs and session state.
    - `client:utils`: Command splitting, path resolution, and validation.
    - `client:prompt`: ANSI-colorized status-aware prompt generation.
    - `client:warp`: Logical VFS navigation logic.
    - `client:peek`: Command translation layer for directory listing.
    - `client:pastevents`: Persistent history management with duplicate detection.
    - `client:input`: Terminal raw-mode handler for arrow-key navigation.

### 2. Modern C++ Adoption
- **C++26 Standards:** Leveraging `std::println`, `std::expected` for functional error handling, and `std::format`.
- **Global Module Fragments:** Migrated standard headers to global module fragments in `.cppm` files to support experimental `g++-16` compilation paths.
- **Architectural Rules:** Strictly enforced ban on `exec*` family system calls to prioritize internal logic over shell-spawning.

### 3. User Experience Enhancements
- **Colorized Shell:** Implemented a status-aware prompt using ANSI escape codes:
    - **Blue:** Username, Hostname, and Logical CWD.
    - **Green (`^_^/*`):** Last command successful.
    - **Red (`v_v/*`):** Last command failed or invalid operation (e.g., pipe attempt).
    - **Yellow (`-_-/*`):** Warning or unknown command.
- **Logical VFS Context:** The client now tracks a virtual current working directory relative to the DNFS root, resolving all paths contextually before transmission.
- **History Navigation:** Implemented terminal raw mode using `termios` to enable **Up/Down arrow** scrolling through previous commands.

## Legacy vs. Modern Comparison

| Feature | Legacy Implementation | Modern Implementation |
| :--- | :--- | :--- |
| **Organization** | Monolithic `.cpp` files | C++20 Modules & Partitions |
| **Error Handling** | Integer return codes | `std::expected<T, E>` |
| **Logging** | `printf` to stdout | Thread-safe file-based `logger` module |
| **Input** | `std::getline` | Raw-mode with Arrow-key support |
| **Path Resolution** | Manual string manipulation | `std::filesystem` & logical VFS context |
| **Build System** | Makefile / Shell scripts | CMake 3.30+ with Ninja |


## Phase 2: Disaggregated Job Servers

- **Architecture**: Decoupled compute from storage (Option 1: Disaggregated JS/SS).
- **Compute Engine**: Created `job-server` module implementing a task-listening service. 
- **Arbitrary Execution**: Implemented a secure `fork`/`exec` compute engine in the Job Server, leveraging relaxed `exec*` rules for the `js:compute` partition.
- **Protocol Expansion**: Added `JS_REGISTER`, `JS_HEARTBEAT`, and `SUBMIT_JOB` protocol types.
- **Client Integration**: Updated CLI with `job <command> [args...] <file>` syntax, supporting robust tokenization (quoted arguments) and status icon feedback.

## Phase 3: DAG Scheduler (Completed)

- **DAG Parsing**: Implemented client-side parser in `client:utils` that converts complex shell strings (`|`, `;`, `&`, `&&`) into a binary `DAGPayload`.
- **Naming Server Orchestration**:
    - **`nm:load_balancer`**: Implemented Policy-based (RoundRobin, Random) JS selection.
    - **`nm:dag_processor`**: Implemented a parallel execution engine using Topological Sort and `std::jthread`.
- **Execution Flow**: NM orchestrates parallel branches simultaneously across multiple Job Servers, while handling linear pipelines sequentially in the current prototype.
- **Verification**: Verified via GTest unit tests for the AST parser and a Docker-based parallelization simulation.

## Legacy vs. Modern Comparison

- **Phase 1 Implementation**: Successfully integrated C-Shell components (prompting, navigation, raw-mode history, colorized status).
- **Phase 2 Architecture**: Evaluated disaggregated vs. co-located models and concluded that **Option 1 (Disaggregated Compute/Storage)** is optimal for maintainability.
- **Job Server Evolution**: Implemented a stateless compute engine using `fork`/`exec` on Job Servers.
- **DAG Execution Discussion**: 
    - Analyzed standard models like Microsoft Dryad and Apache Spark for DAG task orchestration.
    - Determined that the Client will parse shell strings into binary `DAGPayloads` to keep the Naming Server focused on scheduling and load balancing.
    - Resolved that `nm_clt_port` in `js` is for file metadata lookups, ensuring the Job Server can locate data across the Storage Server cluster.
    - Confirmed the use of `client:utils::split_args` to ensure full shell-argument compatibility (e.g., handling quotes) in distributed jobs.
