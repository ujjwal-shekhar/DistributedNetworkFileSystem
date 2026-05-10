# Engineering Standards for C++26 Refactor

## Toolchain

- **Compiler:** `g++-16` (experimental trunk)
- **C++ Standard:** `-std=c++26`
- **Flags:** `-freflection`, `-Wall`, `-Wextra`, `-Werror`, `-fno-plt`

## Programming Guidelines

- Prefer template metaprogramming and reflection over runtime polymorphism.
- Strictly RAII. Use `std::unique_ptr` for ownership. No raw `malloc`/`free` or `new`/`delete`.
- Use C++20/23 spans (`std::span`) for buffer management to avoid copies.
- Prefer using modules, with partitions for their internals and appropriate transitive imports to only expose the API needed and nothing else, and hide all the other stuff inside partitions like internals, networks; etc.
- For networking, make a module that will wrap over Socket API. This should be the networking partition of the corresponding module.
- We will have top level modules as, client, naming server, the naming server spawns and handles the storage server.
- The Makefile is too old, not well organized, and the `clean_compile.sh` is not a good design choice either. Upgrade that to cmake.
- Don't add dependencies, I want to do this without any external libraries.
- Use 2 spaces for indentation.
- Avoid `std::endl` (use `\n`).
- A lot of the old C-style string handling is being done here. Upgrade it to modern `string_view` where possible, fallback to `std::string` otherwise.
- You CANNOT use `exec*` directly anywhere in the project EXCEPT the `job-server`. This applies to all system calls in the exec family.
- Prefer `consteval` over `constexpr`, and `constexpr` over others.
- The current design makes the client "logout" in a way by exiting the executable, implement some form of rate-limiting with limited tokens, with each job having corresponding tokens. The naming server obviously maintains the limits for each client.
- The client has a special admin mode for which a password should be required. For now, let it be a hardcoded value that the naming server checks for. The admin can send "fail server" queries and has infinite tokens.
- The `utils` folder shouldn't exist post legacy to modern upgrade. The corresponding bits would become part of modules. Also, for `logging` we should probably have a top level logger module that other internal partitions will import.
- Use static reflection to automate the parsing of command tokens. If a new command is added to the Admin partition, the parser should update automatically via a reflected enum class of commands. ONLY use the [latest P2996 proposal](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r13.html) for syntax, best practices in reflection and API.
- For functionality normally provided by libraries (like JSON parsing or serialization), the agent must implement a minimal, high-performance version using C++26 reflection and std::expected.
- Separate the module interface and implementation of logically different functions into different files for quicker compilation.
- Replace the starvation-prone rwlock with std::shared_mutex or implement a fair-share semaphore. For high-contention paths, explore std::atomic based lock-free state updates.
- Use modern concurrency features for clean and safe thread spawning/destruction, atomicity guarantees.
- Make the `README.md` even better by making highlighting each feature and a diagram of the workflow for each command, and rationale for the design decisions
- Don't hardcode values like the IP addresses and ports. Try to use ephemeral ports and use messaging to agree on the auto-picked values. Only hardcode when absolutely necessary and always provide an option to change ip/port using cmd line args.

## Agent Behavior

- **Plan First:** Always output a "Refactor Plan" before modifying files.
- **Verification:** After every file modification, attempt to compile the specific file.
- **Safety:** Never use `sudo` or modify files outside of the project root.
- **ASK BEFORE ADDING NEWER FEATURES:** Some features aren't complete here yet. While there is *some* redundancy support, there isn't really failover support here yet. Only when the current system is moved from legacy to modern and has good test coverage with all passing should we move to adding new features.
- **To build:** Do `ninja -C build`, don't try to invoke `/snap/bin/cmake` and NEVER invoke `/usr/bin/cmake`.

## Tests

- Each module X should have a corresponding test file X_test.cpp. Use Catch2 or GTest (since you're open to test libraries) to test module interfaces. For internal partitions, use 'Friendship' or test through the public API to ensure the binary remains thin.
- Use static_assert and consteval validation within module partitions to enforce architectural invariants (e.g., ensuring a storage server never attempts to self-register without a valid Naming Server IP) at compile time.
- Figure out a way to do automated testing for many servers, implement a way to send "FAIL" signals to storage servers for an admin in this test.
- Use Docker Compose to define a multi-container network. This allows the Agent to spawn a 'Naming Server' container and multiple 'Storage Server' containers on a virtual network (e.g., 172.18.0.x). This tests the Socket API logic against real IP routing without needing multiple physical devices.
- I am open to using libraries for testing ONLY, not for the actual network file system service.

## Module Standards (C++26)

- **Global Module Fragment (GMF):** All standard library and system headers (#include) MUST be placed in the Global Module Fragment at the top of the file (before the 'export module' declaration).
- **No 'import <header>':** Avoid using 'import <header>' (header units). Use #include in the GMF instead to maintain full control over the preprocessor state and avoid uncontrolled legacy issues.
- **Module Partitions:** Use private/internal module partitions (e.g., 'module network:internal;') to hide implementation details and third-party/system-level dependencies (like POSIX sockets) from the primary module interface.
- **Transitive Imports:** Use 'export import' sparingly to aggregate modules only when it simplifies the public API for the consumer.

## Refinement of Standards (May 2026 Update)

- **C++26 Reflection (P2996 r13+):**
  - Use the `<meta>` header.
  - Use the `^^` operator for reflection.
  - Wrap the return value of reflection queries (like `std::meta::enumerators_of()`) in `std::define_static_array` to allow for constexpr/static iteration, as they return `std::vector<std::meta::info>`.
  - Use `[: ... :]` for splicing.
- **Module Architecture:**
  - **Dependency Direction:** The Primary Module Interface MUST import its partitions (e.g., `import :internal;`). Implementation units (`.cpp`) import the primary module.
  - **Export Style:** Use individual `export` keywords for granular API control.
- **Naming Server Privilege Tiers:**
  - **Tier 1 (USER):** Read-only/Metadata (e.g., `READ_FILE`, `LIST_ALL`).
  - **Tier 2 (PRIVILEGED):** Structural changes handled by NS instructing SS (e.g., `CREATE_FILE`, `DELETE_DIR`).
  - **Tier 3 (ADMIN):** Critical actions (e.g., `FAIL_SERVER`). Admin only.
- **Code Cleanliness:**
  - Eliminate redundant comments that describe language syntax (e.g., "// Global Module Fragment").
  - **Core Module Removal:** The project no longer uses a dedicated `core` module for bundling standard headers. All modules must include necessary standard library and system headers directly in their Global Module Fragment (GMF) to ensure better isolation and follow modern C++ module practices.

- **Module Partition Dependency Rule:** A module partition (e.g., 'module M:P;') MUST NEVER import the primary module interface ('import M;'). This creates a circular dependency. Instead, the primary module interface should import its partitions.
- **Avoid Hardcoding:** Configuration values like ports, IP addresses, and timeouts MUST be part of a configuration object (e.g., 'Config' struct) or passed as arguments. Never hardcode these in implementation logic.
- **Folder Structure:** Large modules (like Naming Server) should be organized into sub-folders (e.g., 'service/', 'auth/', 'network/') to maintain a clean workspace.
- **File Descriptor Validation:** Use a standardized check for invalid file descriptors (e.g., a constant 'INVALID_FD' or a helper function) rather than magic numbers like '-1'.
- **Cache Optimization:** The LRU cache in the Naming Server must implement a two-tier lookup: a fast O(1) cache check followed by a Trie fallback for missed keys.
