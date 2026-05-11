# OrionFS Roadmap & TODOs

## High-Priority Enhancements

### 1. C++26 Reflection
- [ ] Implement automated serialization for `commands::FilePacket` and `commands::ClientRequest` using C++26 reflection to replace manual struct handling.
- [ ] Explore reflection-based introspection for the `NamingServer`'s trie to simplify metadata logging and state export.

### 2. Argument Parsing Module
- [ ] Create a dedicated `arg_parser` module to standardize CLI argument handling.
- [ ] Refactor `nm`, `ss`, `js`, and `clt` to utilize the new `arg_parser` for robust input validation and flag management.

### 3. Testing & Stability
- [ ] Expand the integration test suite to include automated fault injection scenarios.
- [ ] Increase unit test coverage for the `network` module, specifically covering edge cases for socket closures and partial sends/receives.
