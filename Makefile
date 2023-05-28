TARGET_SERVER = pop3d
SERVER_SOURCES = src/*.c
SERVER_OBJS = src/*.o

COMPILER = ${CC}

CFLAGS = --std=c11 -fsanitize=address -pedantic -pedantic-errors -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200112L -g
HEADERS = -I src/include
LIBS = -l pthread

all: main

clean:
	rm -f $(SERVER_OBJS) $(TARGET_SERVER)

main: 
	$(COMPILER) $(CFLAGS) $(HEADERS) -o $(TARGET_SERVER) $(SERVER_SOURCES) $(LIBS)
		rm -f $(SERVER_OBJS)

.PHONY : all clean