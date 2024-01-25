CXX = clang
CXXFLAGS = -Wall -std=c++20 -Wno-microsoft-cast -target x86_64-windows -nostdlib -ffreestanding -mno-stack-arg-probe \
    -mgeneral-regs-only -fno-asynchronous-unwind-tables -mno-red-zone -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
    -fno-exceptions -fno-rtti -fno-omit-frame-pointer -mcmodel=kernel -I $(INC)

AS = nasm
ASFLAGS = -f win64

LD = lld-link
LFLAGS = -subsystem:native -section:"INIT,RED" -section:".CRT,RED" -section:"PROTDATA,R" -nxcompat:no -incremental:no \
    -fixed:no -dynamicbase:no -nodefaultlib -base:0xffffffff80000000 -entry:x64Entry

INC = ./lib/

OBJECTS = ./lib/libc/mem.o \
./lib/libc/str.o \
./lib/libc/print.o \
./lib/ec/new.o \
./lib/ec/string.o \
./hw/acpi/acpi.o \
./hw/serial/serial.o \
./hw/cpu/cpu.o \
./hw/cpu/isr.o \
./hw/cpu/intctrl.o \
./hw/cpu/x64.o \
./hw/timer/timer.o \
./hw/ps2/ps2.o \
./hw/ps2/keyboard.o \
./core/gfx/output.o \
./core/panic.o \
./core/alloc.o \
./core/thread.o \
./core/init.o

all: kernel.exe
	mv kernel.exe ../bin/Debug/vdisk/EFI/BOOT/kernel.exe

kernel.exe: $(OBJECTS)
	$(LD) $(LFLAGS) $^ -out:$@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# TODO:
# copy to vdisk dir and/or flash drive
# start qemu
# delete temporary files
# make clang generate assembly files
# linker script?

.PHONY: all clean
clean:
	find -type f -name "*.o" -delete -o -name "*.exe" -delete
