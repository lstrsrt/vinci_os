CXX = clang
CXXFLAGS = -Wall -std=c++20 -target x86_64-windows -nostdlib -ffreestanding -fno-asynchronous-unwind-tables -mno-stack-arg-probe -mgeneral-regs-only
LD = lld-link
LFLAGS = -subsystem:efi_application -nodefaultlib -dll -entry:EfiMain
# FIXME - -dll necessary?

all: BOOTX64.EFI
	mv BOOTX64.EFI ../bin/Debug/vdisk/EFI/BOOT/BOOTX64.EFI

BOOTX64.EFI: boot.o
	$(LD) $(LFLAGS) $< -out:$@

boot.o: entry.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean all
clean:
	find -type f -name "*.o" -delete -o -name "*.efi" -delete
