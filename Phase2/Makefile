CC = gcc
CFLAGS = -Og
LDLIBS = -lpthread
OBJECTS = myshell.o
TARGET = shellex

SRCS = myshell.h myshell.c

$(TARGET) : $(OBJECTS)
				$(CC) $(CFLAGS) -o $@ $^
clean:
			rm -rf *. *.o *.lst shellex
