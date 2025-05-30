# Minimal makefile for Scheduler Project

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -pthread
LDFLAGS = -pthread

# .c files list
SRCS = src/main.c \
       src/task.c \
       src/tasks_impl.c \
       src/queue.c \
       src/scheduler.c \
       #src/utils.c

# .o files generation
OBJS = $(SRCS:.c=.o)

#Executable final name
TARGET = scheduler

#Default rules: Compile all
all: $(TARGET)

# how generate exec from .o files
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

#Generic rule: .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) -c  $< -o $@

#Delete objects and executables
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean