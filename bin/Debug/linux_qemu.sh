#!/bin/sh
qemu-system-x86_64 -cpu max -net none -monitor stdio -serial file:serial_qemu.txt -rtc base=localtime -no-reboot -d cpu_reset -D ./qemu.txt -bios OVMF_X64.fd -drive file=fat:rw:vdisk,index=1,format=raw
