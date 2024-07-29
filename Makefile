CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LDFLAGS = -lncurses -lmenu -lm

SRCS = main.c library.c
OBJS = $(SRCS:.c=.o)
DEPS = library.h

TARGET = termzic

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

run:
	./$(TARGET)

clean:
	rm -rf $(OBJS) $(TARGET)
