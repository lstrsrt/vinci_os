all: boot kernel

boot:
	$(MAKE) -C boot

kernel:
	$(MAKE) -C kernel

release:
	$(MAKE) -C boot CONFIG=Release
	$(MAKE) -C kernel CONFIG=Release

# Each fragment ends in ",", so drop the last one and wrap everything in [ ].
compdb:
	$(MAKE) -C boot compdb
	$(MAKE) -C kernel compdb
	{ echo '['; cat boot/build/Debug/boot.o.json $$(find kernel/build/Debug -name '*.o.json' | sort) | sed '$$ s/,$$//'; echo ']'; } > compile_commands.json

.SILENT:
.PHONY: all boot kernel release clean compdb
clean:
	$(MAKE) -C boot clean
	$(MAKE) -C kernel clean
	rm -f bin/*/vdisk/EFI/BOOT/BOOTX64.EFI bin/*/vdisk/EFI/BOOT/kernel.exe compile_commands.json
