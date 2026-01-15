ifndef QCONFIG
	QCONFIG=qconfig.mk
endif
CCFLAGS += -O0 -g3

include $(QCONFIG)

#NAME = $(PROJECT_ROOT)

INSTALLDIR=usr/bin

define PINFO
PINFO DESCRIPTION = Rpi4 Bluetooth stack
endef

include $(MKFILES_ROOT)/qtargets.mk
LIBS += # none

RPI_VERSION = 4  #didn't worked, I manually defined in base.h
