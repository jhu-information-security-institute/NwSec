#
# icmp-reset makefile
#
CC	= gcc
CFLAGS = -Wall -D_BSD_SOURCE
SRC = icmp-reset.c
OBJ = icmp-reset.o
BIN = icmp-reset

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ)

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

clean: 
	-rm -f $(OBJ) $(BIN)
