# username: mingyangz
# student_id: 650242

## CC  = Compiler.
## CFLAGS = Compiler flags.
CC	= gcc
CFLAGS =	-Wall -Wextra -std=gnu99 -lpthread


## OBJ = Object files.
## SRC = Source files.
## EXE = Executable name.

SRC =		server.c log.c sha256.c
OBJ =		server.o log.o sha256.o
EXE = 		server

## Top level target is executable.
$(EXE):	$(OBJ)
		$(CC) $(CFLAGS) -o $(EXE) $(OBJ) -lm


## Clean: Remove object files and core dump files.
clean:
		/bin/rm $(OBJ)

## Clobber: Performs Clean and removes executable file.

clobber: clean
		/bin/rm $(EXE)

## Dependencies

server.o:	log.h sha256.h uint256.h
log.o: log.h
sha256.o: sha256.h
