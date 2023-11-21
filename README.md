# Network File System

## Team mates
- Anika Roy
- Prakul Agarwal
- Ujjwal Shekhar

# Usage instructions
- Clone the repository
```bash
git clone link ./name_of_the_directory
```

- Navigate to the directory
```bash
cd name_of_the_directory
```

- Run the script
```bash
chmod +x clean_compile.sh
./clean_compile.sh
```

# Components
## Naming Server
- Navigate to the directory where NM will start
```bash
./nm
```

## Storage Servers
- Navigate to the directory where server will start
```bash
./server <server_id> <NM_port> <CLT_port>
```

## Clients
- Navigate to the directory where server will start
```bash
./client
```

# Bibliography and Assumptions
- ctrl-Z to exit a client only.
- Writing to a file is ended by a double enter.
- Server ID is to be entered by the person that is inititializing the server.
- Instead of asking for user accessible paths, the server will assume all the directories inside the directory that it is run is accessible by it.
- All paths are unique.
- We need to have atleast `MIN_INIT_SERVERS` number of servers running. 

# Usage of AI tools
ChatGPT-3.5Turbo was used.
