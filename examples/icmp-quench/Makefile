#
# icmp-quench makefile
#
CC	= gcc
CFLAGS = -Wall -D_BSD_SOURCE
SRC = icmp-quench.c
OBJ = icmp-quench.o
BIN = icmp-quench

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

clean: 
	-rm -f $(OBJ) $(BIN)
