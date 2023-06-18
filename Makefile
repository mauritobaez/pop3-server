TARGET_SERVER = pop3d
TARGET_CLIENT = peepclient
SERVER_SOURCES = src/*.c
SERVER_OBJS = src/*.o
CLIENT_SOURCES = peep_client/*.c
CLIENT_OBJS = peep_client/*.o

COMPILER = ${CC}

CFLAGS = --std=c11 -fsanitize=address -pedantic -pedantic-errors -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -g 
SERVER_HEADERS = -I src/include
CLIENT_HEADERS = -I peep_client/include
LIBS = -l pthread

all: main client

clean:
	rm -f $(SERVER_OBJS) $(TARGET_SERVER) $(CLIENT_OBJS) $(TARGET_CLIENT)

main: 
	$(COMPILER) $(CFLAGS) $(SERVER_HEADERS) -o $(TARGET_SERVER) $(SERVER_SOURCES) $(LIBS)
		rm -f $(SERVER_OBJS)

client:
	$(COMPILER) $(CFLAGS) $(CLIENT_HEADERS) -o $(TARGET_CLIENT) $(CLIENT_SOURCES) $(LIBS)
		rm -f $(CLIENT_OBJS)

debug: CFLAGS += $(DEBUG)
debug: main

.PHONY : all clean