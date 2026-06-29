CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

all: server client users

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

users: users.c
	$(CC) $(CFLAGS) -o users users.c

clean:
	rm -f server client users