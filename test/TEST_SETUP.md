# rpi4BT Google Test Setup

## Overview
Google Test (gtest) framework has been integrated into the rpi4BT project following the same pattern used in the rpi4-system-monitor project for QNX OS.

## Test Structure

### Directory Created
- `/home/iismail/qnx/src/jamal/rpi4BT/test/` - Contains all test files

### Files Created
1. **Makefile** - QNX-based build configuration
   - Uses `qcc` compiler with aarch64 architecture
   - Compiles C source files with C++ test framework
   - Links against gtest libraries: `-lgtest`, `-lgtest_main`, `-lregex`, `-lc++`

2. **rpi4bt_test.cpp** - Test suite with comprehensive tests for 5 key functions
   - Tests 15+ test cases covering normal and edge cases

3. **run_rpi4bt_tests** - Executable script to build and run tests

## Functions Tested

### 1. `btBuffer_create()`
**Location:** [btQueue.c](../btQueue.c)
**Tests:**
- Basic memory allocation
- Correct size initialization
- Various buffer sizes (1, 64, 256, 1024, 4096 bytes)
- Edge case: zero-size buffer

### 2. `btBuffer_destroy()`
**Location:** [btQueue.c](../btQueue.c)
**Tests:**
- Memory deallocation
- NULL pointer handling

### 3. `btQueue_init()`
**Location:** [btQueue.c](../btQueue.c)
**Tests:**
- Queue pointer initialization
- Multiple initialization calls
- Proper clearing of queue state

### 4. `btQueue_enqueueBuffer()`
**Location:** [btQueue.c](../btQueue.c)
**Tests:**
- Single buffer enqueueing
- Multiple buffer enqueueing with proper linking
- NULL buffer handling
- Queue pointer management

### 5. `btQueue_dequeue()`
**Location:** [btQueue.c](../btQueue.c)
**Tests:**
- Dequeueing from empty queue
- Single buffer dequeue
- Multiple buffer dequeue (FIFO order)
- NULL queue handling
- Queue state verification after dequeue

### Integration Tests
- **QueueFifoOrder** - Verifies FIFO (First-In-First-Out) behavior across multiple operations

## Build Instructions

```bash
cd /home/iismail/qnx/src/jamal/rpi4BT/test
make          # Build the test executable
make run      # Run the tests
make clean    # Clean build artifacts
```

Or use the convenience script:
```bash
./run_rpi4bt_tests
```

## Test Compilation Details

The Makefile:
- Compiles C source files (`btQueue.c`, `utils.c`) with C compiler
- Compiles test file (`rpi4bt_test.cpp`) with C++ compiler
- Links everything together with gtest framework
- Creates executable: `run_rpi4bt_tests`

## QNX-Specific Configuration

Following the rpi4-system-monitor pattern:
- **Compiler:** `qcc` (QNX Compiler Collection)
- **Platform:** `-Vgcc_ntoaarch64le` (64-bit ARM for RPi4)
- **C++ Support:** `-lang-c++` flag for C++ compilation
- **Regex Support:** `-lregex` required by gtest on QNX
- **C++ Standard Library:** `-lc++` for libc++

## Next Steps

You can now:
1. Build and run the tests: `cd test && ./run_rpi4bt_tests`
2. Add more test cases following the same pattern
3. Test additional functions from other source files (hci.c, l2cap.c, etc.)
4. Integrate this into your CI/CD pipeline

## Reference

This setup mirrors the approach used in:
- `/home/iismail/qnx/src/jamal/rpi4-system-monitor/test/`

See that directory for additional examples of test patterns and assertions.
