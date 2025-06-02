all:
	make -C boot
	make -C kernel

boot:
	make -C boot

kernel:
	make -C kernel

.SILENT:
.PHONY: all clean
clean:
	find -type f -name "*.o" -delete -o -name "*.exe" -delete -o -name "*.EFI" -delete