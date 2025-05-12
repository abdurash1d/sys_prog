# Determine the OS
ifeq ($(OS),Windows_NT)
PLATFORM = WINDOWS
RM = del /Q
EXE = .exe
else
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
PLATFORM = LINUX
RM = rm -f
EXE =
endif
ifeq ($(UNAME_S),Darwin)
PLATFORM = MACOS
RM = rm -f
EXE =
endif
endif

# Compiler and flags
CC = gcc
TARGET = cpu_memory_tracker$(EXE)

# Platform-specific flags and libraries
ifeq ($(PLATFORM),WINDOWS)
CFLAGS = -Wall -Wextra -O2 `pkg-config --cflags gtk+-3.0` -D_WIN32
LDFLAGS = `pkg-config --libs gtk+-3.0` -lpthread -lpsapi
else ifeq ($(PLATFORM),MACOS)
CFLAGS = -Wall -Wextra -O2 `pkg-config --cflags gtk+-3.0` -D__APPLE__
LDFLAGS = `pkg-config --libs gtk+-3.0` -pthread
else ifeq ($(PLATFORM),LINUX)
CFLAGS = -Wall -Wextra -O2 `pkg-config --cflags gtk+-3.0` -DLINUX
LDFLAGS = `pkg-config --libs gtk+-3.0` -pthread
endif

# Source files
SRCS = main.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TARGET)
