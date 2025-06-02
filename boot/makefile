CXX = clang
CXXFLAGS = -Wall -std=c++20 -target x86_64-windows -nostdlib -ffreestanding -fno-asynchronous-unwind-tables -mno-stack-arg-probe -mgeneral-regs-only
LD = lld-link
LFLAGS = -subsystem:efi_application -nodefaultlib -dll -entry:EfiMain
# FIXME - -dll necessary?

INCLUDE = ../kernel/lib/

all: BOOTX64.EFI
	pushd ../bin/Debug/vdisk && \
	mkdir -p EFI/BOOT && \
	popd && \
	mv BOOTX64.EFI ../bin/Debug/vdisk/EFI/BOOT/BOOTX64.EFI

BOOTX64.EFI: boot.o
	$(LD) $(LFLAGS) $< -out:$@

boot.o: entry.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.SILENT:
.PHONY: all clean
clean:
	find -type f -name "*.o" -delete -o -name "*.EFI" -delete
