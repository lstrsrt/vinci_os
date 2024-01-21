CXX = clang
CXXFLAGS = -Wall -std=c++20 -target x86_64-windows -nostdlib -ffreestanding -fno-asynchronous-unwind-tables -mno-stack-arg-probe -mgeneral-regs-only
LD = lld-link
LFLAGS = -subsystem:efi_application -nodefaultlib -dll -entry:EfiMain
# FIXME - -dll necessary?

all: boot.efi

boot.efi: boot.o
	$(LD) $(LFLAGS) $< -out:$@

boot.o: entry.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean all
clean:
	find -type f -name "*.o" -delete -o -name "*.efi" -delete
