# Set PRG_SUFFIX_FLAG below to either 0 or 1 to enable or disable
# the generation of a .exe suffix on executables
PRG_SUFFIX_FLAG := 0

LDFLAGS := -pthread
CFLAGS_INC := -Iutils  # Add your include directories here
CFLAGS := -g -Wall $(CFLAGS_INC)

# Update source file locations
SRCS := $(wildcard NamingServer/*.c Clients/*.c StorageServers/*.c utils/*.c)

# Naming Server
NM_SRC := $(wildcard NamingServer/nm.c)
NM_OBJS := $(patsubst %.c,%.o,$(NM_SRC))
NM_BIN := nm

# Clients
CLIENT_SRC := $(wildcard Clients/client.c)
CLIENT_OBJS := $(patsubst %.c,%.o,$(CLIENT_SRC))
CLIENT_BIN := client

# Storage Servers
SERVER_SRC := $(wildcard StorageServers/server.c)
SERVER_OBJS := $(patsubst %.c,%.o,$(SERVER_SRC))
SERVER_BIN := server

# All objects
OBJS := $(filter-out $(NM_OBJS) $(CLIENT_OBJS) $(SERVER_OBJS), $(patsubst %.c,%.o,$(SRCS)))

all: $(NM_BIN) $(CLIENT_BIN) $(SERVER_BIN)

# Compile Naming Server
$(NM_BIN): $(NM_OBJS) $(OBJS)
	$(CC) $(NM_OBJS) $(OBJS) $(LDFLAGS) -o $(NM_BIN)

# Compile Clients
$(CLIENT_BIN): $(CLIENT_OBJS) $(OBJS)
	$(CC) $(CLIENT_OBJS) $(OBJS) $(LDFLAGS) -o $(CLIENT_BIN)

# Compile Storage Servers
$(SERVER_BIN): $(SERVER_OBJS) $(OBJS)
	$(CC) $(SERVER_OBJS) $(OBJS) $(LDFLAGS) -o $(SERVER_BIN)

# Cleanup
clean:
	$(RM) $(NM_BIN) $(CLIENT_BIN) $(SERVER_BIN)

veryclean: clean
	$(RM) $(NM_OBJS) $(CLIENT_OBJS) $(SERVER_OBJS)

rebuild: veryclean all
