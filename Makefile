CC = g++
AR = ar
CFLAGS = -Wall -c -o 
# List of source files
SRCS = uthreads.cpp demo_jmp.c # Add more .c files if there are additional source files

# Generate names of object files from source files
OBJS = uthreads.o demo_jmp.o

# Default target
all: libuthreads.a

# Rule to build object files
demo_jmp.o: demo_jmp.c
	$(CC) $(CFLAGS) demo_jmp.o demo_jmp.c

# Rule to build object files
uthreads.o: uthreads.cpp
	$(CC) $(CFLAGS) uthreads.o uthreads.cpp

# Rule to build the static library
libuthreads.a: $(OBJS)
	$(AR) -rcs $@ $(OBJS)

# Clean target to remove generated files
clean:
	rm -f $(OBJS) libuthreads.a

tar:
	tar -cf ex2.tar README $(SRCS) Makefile 