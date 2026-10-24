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
