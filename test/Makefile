CC = qcc
CXX = qcc
QCC_PLATFORM := -Vgcc_ntoaarch64le

TARGET := run_rpi4bt_tests
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj

CPPFLAGS += -I.. -I../public
CFLAGS += $(QCC_PLATFORM) -O0 -g3 -Wall
CXXFLAGS += $(QCC_PLATFORM) -lang-c++ -O0 -g3 -Wall
LDFLAGS += $(QCC_PLATFORM)
LDLIBS += -lgtest -lgtest_main -lregex -lc++ -lm

C_SRCS := \
	../btQueue.c \
	../utils.c
CPP_SRCS := rpi4bt_test.cpp

C_OBJS := $(C_SRCS:../%.c=$(OBJ_DIR)/%.o)
CPP_OBJS := $(CPP_SRCS:%.cpp=$(OBJ_DIR)/%.o)
OBJS := $(C_OBJS) $(CPP_OBJS)
DEPS := $(OBJS:.o=.d)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: ../%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)
