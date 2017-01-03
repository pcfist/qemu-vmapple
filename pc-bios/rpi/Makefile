all: build-all
# Dummy command so that make thinks it has done something
	@true

include ../../config-host.mak
include $(SRC_PATH)/rules.mak

$(call set-vpath, $(SRC_PATH)/pc-bios/rpi)

.PHONY : all clean build-all

OBJECTS = start.o main.o uart.o
QEMU_CFLAGS := $(filter -W%, $(QEMU_CFLAGS))
QEMU_CFLAGS += -ffreestanding -fno-delete-null-pointer-checks
QEMU_CFLAGS += -fPIE -fno-strict-aliasing
QEMU_CFLAGS += $(call cc-option, $(QEMU_CFLAGS), -fno-stack-protector)
QEMU_CFLAGS += -flto
LDFLAGS += -Wl,--section-start=.text=0x10000000 -nostdlib -flto

build-all: rpi-aa64.img

rpi-aa64.elf: $(OBJECTS)
	$(call quiet-command,$(CC) $(LDFLAGS) -o $@ $(OBJECTS),"BUILD","$(TARGET_DIR)$@")

rpi-aa64.img: rpi-aa64.elf
	$(call quiet-command,strip --strip-unneeded $< -o $@,"STRIP","$(TARGET_DIR)$@")

$(OBJECTS): Makefile

clean:
	rm -f *.o *.d *.img *.elf *~
