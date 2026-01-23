# Compiler for QNX
CXX = qcc
CC = qcc

# Target Architecture for RPi4 (64-bit ARM)
PLATFORM = -Vgcc_ntoaarch64le

# Compiler Flags
# For C code: standard C compilation
CFLAGS = $(PLATFORM) -Wall -I../

# For C++ test code: C++ compilation with gTest
CXXFLAGS = $(PLATFORM) -lang-c++ -Wall -I../

# GoogleTest Libraries provided by QNX
# -lgtest_main provides the main() entry point for the test
# -lgtest is the core framework
# -lregex is required by libgtest on QNX
# -lc++ for libc++
LIBS = -lgtest -lgtest_main -lregex -lc++

# Output binary name
TARGET = run_rpi4bt_tests

# Source files
CPP_SRCS = rpi4bt_test.cpp
C_OBJS = btQueue.o utils.o
CPP_OBJS = rpi4bt_test.o
OBJS = $(C_OBJS) $(CPP_OBJS)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

# Compile C source files
btQueue.o: ../btQueue.c ../btQueue.h
	$(CC) $(CFLAGS) -c ../btQueue.c -o $@

utils.o: ../utils.c ../utils.h
	$(CC) $(CFLAGS) -c ../utils.c -o $@

# Compile C++ test file
rpi4bt_test.o: $(CPP_SRCS)
	$(CXX) $(CXXFLAGS) -c $(CPP_SRCS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all run clean
