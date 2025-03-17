name = reverse-udp-tunnel
objs = conn_table.c sha256.c hashmap.c outside.c args.c inside.c main.c mac.c misc.c
CFLAGS = -O3
LFLAGS = -lssl -lcrypto
CC = gcc

$(name): $(objs)
	$(CC) $(CFLAGS) -o $(name) $(objs) $(LFLAGS)

clean:
	rm -f $(obs) $(name)


