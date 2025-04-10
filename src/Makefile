# Define the compiler and flags
CC = gcc
CFLAGS = -Wall -pthread

# Define the source files and object files
SRC = chash.c functions.c
OBJ = chash.o functions.o

# Define the header file
IncludeFile = ./../include/functions.h

# Define the target executable
EXE = myprogram

# Default rule
all: run

# Rule to build the executable
$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)

# Rule to compile chash.o from chash.c
chash.o: chash.c $(IncludeFile)
	$(CC) $(CFLAGS) -c chash.c

# Rule to compile functions.o from functions.c
functions.o: functions.c $(IncludeFile)
	$(CC) $(CFLAGS) -c functions.c

# Rule to run the program after building it
run: $(EXE)
	./$(EXE)


# Clean the build artifacts
clean:
	rm -f $(OBJ) $(EXE) output.txt
