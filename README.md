# vinci_os

## Kernel

* ASCII cat (Vinci) on startup
* Thread scheduler (with ring 0/3 switching)
* Native syscall/sysret support
* Detects and enables other x64 things (SMAP and UMIP)
* Parses static ACPI tables
* Includes drivers for PIT, HPET, PIC, APIC (incomplete), PS/2 keyboard and serial port

![kernel](https://github.com/lstrsrt/os64/assets/79076531/fe3d3c87-90fb-4902-ba26-c04db0e86d6b)
<img width="848" height="712" alt="Screenshot From 2026-10-24 13-14-42" src="https://github.com/user-attachments/assets/dfef42da-e551-4764-b7d6-72727087471b" />

## Bootloader

* Loads the kernel... that's it

![bootloader](https://github.com/lstrsrt/os64/assets/79076531/b0df5c3f-4066-4920-a3f1-64dd28353de7)
=======

## Building & Running

### Linux

You need `clang` and `nasm` to build, and QEMU (recommended) or VirtualBox if you want to run the OS.
If using VirtualBox, you additionally need `sfdisk` (from util-linux), `mkfs.fat` (from dosfstools)
and `mtools` in order to create a bootable image.

On Arch Linux:
`sudo pacman -S --needed clang nasm util-linux dosfstools mtools qemu-full`

Simply run `make all` from the project root, or `make release` to build with optimizations.

QEMU needs no further setup, just run `linux_qemu.sh` from `bin/Debug` or `bin/Release`.
For VirtualBox, see the final section below.

### Windows

Install [Visual Studio](https://visualstudio.microsoft.com/downloads), or just the VS Build Tools if you don't need the IDE.
Select the "Desktop Development with C++" workload in the installer.
Then install the [VSNASM](https://github.com/ShiftMediaProject/VSNASM) extension by cloning the repo and running `install_script.bat` in an elevated developer command prompt.
To build, either use the IDE or run `msbuild os64.sln` in the project root from a developer prompt. Append `/p:Configuration=Release` for optimizations.

For QEMU, [install](https://qemu.org/download) it and `run_qemu.bat` from `bin/Debug` or `bin/Release`.
See section below for details on VirtualBox.

### Running in VirtualBox

For VirtualBox, you need to create a VM called "VinciOS". Set "OS" to "Other", allocate at least 128MB of RAM and enable "Use EFI".

On Windows, give it a VHD disk and mount it via Disk Management (`diskmgmt.msc`). Format it with a GPT partition table and a FAT filesystem,
copy over the EFI folder from `bin/{Debug,Release}/vdisk` and detach it again.

On Linux, running `bin/{Debug,Release}/make_vbox_disk.sh` will create a bootable VDI for the VM.

Start the VM from the UI or with `VirtualBoxVM --startvm VinciOS`.
