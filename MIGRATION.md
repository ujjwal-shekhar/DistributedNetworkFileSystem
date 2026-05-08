#C to C++ 26 Migration Plan(Strangler Fig Approach)

## Background & Motivation
The project currently consists of a C codebase managed by a `Makefile`. To improve maintainability, type safety, and leverage modern paradigms, we will migrate the system to a modular C++26 architecture using `CMake`. We will strictly adhere to C++26 Module semantics, avoiding traditional header inclusions where possible.

## Scope & Impact
This migration encompasses the NamingServer, StorageServers, Clients, and shared utility modules. The architecture will shift from monolithic C scripts to modern, decoupled C++26 modules. The chosen "Strangler Fig" pattern ensures the project remains functional at all times throughout the transition. External libraries are strictly forbidden for core application logic; they are only permitted for testing (e.g., GoogleTest).

## Proposed Solution: Strangler Fig with C++26 Modules
1. **New Environment:** Establish a new C++26 CMake environment capable of scanning and compiling C++ modules (`.cppm` / `.ixx`).
2. **FFI Wrappers:** Initially compile the existing C files as isolated static C libraries and link them to C++ entry points via `extern "C"`.
3. **Module Encapsulation:** Methodically replace the internal implementation with native C++26 Modules (`export module <name>;`). 
4. **Internal Partitions:** Hide implementation details—especially raw C POSIX APIs—inside private module partitions (e.g., `module <name>:internal;`).
5. **Transitive Imports:** Utilize `export import` for cohesive module aggregation where appropriate, minimizing duplicate imports across dependent modules.

## Implementation Phases

### Phase 1: Build System & Foundation
- **Goal:** Establish the CMake skeleton and hook up the existing C code.
- **Steps:**
  1. Create a top-level `CMakeLists.txt` requiring CMake 3.28+ (for robust C++ module support) and C++26 standard (`CMAKE_CXX_STANDARD 26`).
  2. Define static C library targets for existing sources.
  3. Create lightweight C++ executable targets (`nm_cpp`, `client_cpp`, `server_cpp`) that contain a C++ `main()` function, delegating execution back to the original C entry points.
- **Milestone:** The system builds via CMake and runs exactly as it did under the `Makefile`.

### Phase 2: Testing Infrastructure & Baseline
- **Goal:** Introduce C++ testing frameworks before rewriting business logic.
- **Steps:**
  1. Integrate a testing framework (e.g., GoogleTest or Catch2) using CMake's `FetchContent`.
  2. Write C++ tests targeting the existing C API.
  3. Add high-level integration tests simulating Client-NamingServer-StorageServer communication.
- **Milestone:** CI/Local testing commands (e.g., `ctest`) are green. Test coverage provides a safety net for upcoming rewrites.

### Phase 3: Component Strangulation (The C++26 Rewrite)
- **Goal:** Gradually rewrite parts of the system utilizing pure C++26 modules.
- **Steps:**
  1. **Utils Module (`utils/`):** Create `export module utils;
`.Expose modern features(`<expected>`, `std::string_view`) and
    wrap legacy structs.Use module partitions to hide legacy C structures.2. *
        *Network Module : ****CRITICAL STEP.**Create a pristine C
                                             ++ 26 `export module network;
`. 
      - This module will act as a modern object-oriented wrapper over the raw C POSIX Socket API. 
      - The raw `<sys/socket.h>` and `<arpa/inet.h>` includes and file descriptor manipulations will be strictly hidden inside private module partitions (e.g., `module network:internal;`).
      - Other modules will consume this network module instead of writing raw socket logic.
  3. **Clients Module (`Clients/`):** Rebuild the client communication layer into `export module client;
`.Import the `network` and `utils` modules to handle requests.4. *
        *Storage Servers(`StorageServers /`)
    : **Migrate threading
    and concurrency to C
    ++ 26 standards(`std::jthread`, `<mutex>`) under `export module storage;
`.Delegate network traffic to the `network` module.5. *
        *Naming Server(`NamingServer /`)
    : **Port the routing
    and directory management structures to robust C
    ++ containers under `export module naming;
`.- **Milestone : **The old C libraries are phased out.All logic operates
                      natively in C++ 26 Modules,
    utilizing safe POSIX Socket API wrappers.

        ## #Phase 4 : Cleanup -
        **Goal : **Finalize the modernization.-
        **Steps
    : **1. Delete the
          legacy `Makefile`.2. Remove any remaining `extern "C"` blocks and `
              .c` files.3. Enable strict C++ compiler warnings
              .

      ##Verification &Testing Test coverage will grow from the outside
          in.By establishing the integration tests in Phase 2,
    we guarantee behavioral equivalence as we replace the underlying C
            implementations in Phase 3. New C++ modules will receive dedicated
                unit tests.

        ##Migration &Rollback Strategy The `Makefile` remains untouched in
            Phase 1 and
        2. If the CMake setup exhibits issues,
    developers can revert to running `make all`
        .The transition is finalized in Phase 4 once the module pipeline is
            completely proven.