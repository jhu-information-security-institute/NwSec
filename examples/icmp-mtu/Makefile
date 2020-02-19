#
# icmp-mtu makefile
#
CC	= gcc
CFLAGS = -Wall -D_BSD_SOURCE
SRC = icmp-mtu.c
OBJ = icmp-mtu.o
BIN = icmp-mtu

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

clean: 
	-rm -f $(OBJ) $(BIN)
