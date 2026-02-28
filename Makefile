CC = qcc
CXX = qcc
QCC_PLATFORM := -Vgcc_ntoaarch64le

PROJECT := rpi4BT

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
TARGET := $(BIN_DIR)/$(PROJECT)

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

.PHONY: all clean install uninstall test test-run test-clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

install: $(TARGET)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)$(BINDIR)/$(PROJECT)
	$(INSTALL) -d $(DESTDIR)$(INCLUDEDIR)
	$(INSTALL) -m 0644 public/rpi4bt/rpi4bt_msg.h $(DESTDIR)$(INCLUDEDIR)/rpi4bt_msg.h

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROJECT)
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
