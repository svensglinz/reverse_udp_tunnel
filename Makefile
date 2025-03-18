name = reverse-udp-tunnel
objs = conn_table.o sha256.o hashmap.o outside.o args.o inside.o main.o mac.o misc.o
CFLAGS = -O3 -Iinclude
LFLAGS = -lssl -lcrypto
CC = gcc
BIN_DIR = bin

$(BIN_DIR)/$(name): $(addprefix $(BIN_DIR)/, $(objs))
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS)

$(BIN_DIR)/%.o: src/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f $(BIN_DIR)/*.o $(BIN_DIR)/$(name)
