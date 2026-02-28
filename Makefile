CC = qcc
CXX = qcc
QCC_PLATFORM := -Vgcc_ntoaarch64le

PROJECT := rpi4BT
CLI_APP := rpi4bt-cli
HID_APP := rpi4bt-hid

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TARGET := $(BIN_DIR)/$(PROJECT)
CLI_TARGET := $(BIN_DIR)/$(CLI_APP)
HID_TARGET := $(BIN_DIR)/$(HID_APP)
APP_TARGETS := $(CLI_TARGET) $(HID_TARGET)

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
INCLUDEDIR ?= $(PREFIX)/include/rpi4bt
INSTALL ?= install

CPPFLAGS += -I. -Ipublic
CFLAGS += $(QCC_PLATFORM) -O0 -g3 -Wall
LDFLAGS += $(QCC_PLATFORM)
LDLIBS +=

SRCS := \
	btQueue.c \
	client.c \
	hci.c \
	hci_events.c \
	hid.c \
	iomsg.c \
	l2cap.c \
	menu.c \
	resmgr.c \
	rfcomm.c \
	rpi4bt.c \
	sdp.c \
	sdp_parser.c \
	transport.c \
	transport_serial.c \
	utils.c
#   transport_uart.c
	

OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
CLI_OBJ := $(OBJ_DIR)/apps/rpi-cli/rpi_cli.o
HID_OBJ := $(OBJ_DIR)/apps/rpi-hid/rpi_hid.o
APP_DEPS := $(CLI_OBJ:.o=.d) $(HID_OBJ:.o=.d)

.PHONY: all clean install uninstall test test-run test-clean

all: $(TARGET) $(APP_TARGETS)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(CLI_TARGET): $(CLI_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(HID_TARGET): $(HID_OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(PROJECT)
	$(INSTALL) -m 0755 $(CLI_TARGET) $(DESTDIR)$(BINDIR)/$(CLI_APP)
	$(INSTALL) -m 0755 $(HID_TARGET) $(DESTDIR)$(BINDIR)/$(HID_APP)
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 0644 public/rpi4bt/rpi4bt_msg.h $(DESTDIR)$(INCLUDEDIR)/rpi4bt_msg.h

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROJECT)
	rm -f $(DESTDIR)$(BINDIR)/$(CLI_APP)
	rm -f $(DESTDIR)$(BINDIR)/$(HID_APP)
	rm -f $(DESTDIR)$(INCLUDEDIR)/rpi4bt_msg.h

test:
	$(MAKE) -C test

test-run:
	$(MAKE) -C test run

test-clean:
	$(MAKE) -C test clean

clean: test-clean
	rm -rf $(BUILD_DIR)

-include $(DEPS)
-include $(APP_DEPS)
