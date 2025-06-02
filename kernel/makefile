CXX = clang
CXXFLAGS = -std=c++23 -Wall -Wno-unused-function -Wno-deprecated-volatile -target x86_64-windows -nostdlib \
    -ffreestanding -mno-stack-arg-probe -mgeneral-regs-only -fno-asynchronous-unwind-tables -mno-red-zone \
    -mno-80387 -mno-mmx -mno-sse -mno-sse2 -fno-exceptions -fno-rtti -fno-omit-frame-pointer -mcmodel=kernel \
    -I $(INCLUDE)

AS = nasm
ASFLAGS = -f win64

LD = lld-link
LFLAGS = -subsystem:native -section:"INIT,RED" -section:".CRT,RED" -section:"PROTDATA,R" -nxcompat:no -incremental:no \
    -fixed:no -dynamicbase:no -nodefaultlib -base:0xffffffff80000000 -entry:x64Entry

INCLUDE = ./lib/

OBJECTS = ./core/alloc.o \
./core/init.o \
./core/panic.o \
./core/thread.o \
./lib/ec/new.o \
./lib/ec/string.o \
./lib/libc/mem.o \
./lib/libc/print.o \
./lib/libc/str.o \
./hw/acpi/acpi.o \
./hw/cpu/cpu.o \
./hw/cpu/isr.o \
./hw/cpu/intctrl.o \
./hw/cpu/x64.o \
./hw/gfx/output.o \
./hw/nvme/nvme.o \
./hw/pci/pci.o \
./hw/ps2/keyboard.o \
./hw/ps2/ps2.o \
./hw/serial/serial.o \
./hw/timer/timer.o \

all: kernel.exe
	mv kernel.exe ../bin/Debug/vdisk/EFI/BOOT/kernel.exe

kernel.exe: $(OBJECTS)
	pushd ../bin/Debug/vdisk && \
	mkdir -p EFI/BOOT && \
	popd && \
	$(LD) $(LFLAGS) $^ -out:$@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# TODO:
# start qemu
# delete temporary files
# make clang generate assembly files

.SILENT:
.PHONY: all clean
clean:
	find -type f -name "*.o" -delete -o -name "*.exe" -delete
