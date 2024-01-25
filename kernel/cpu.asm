extern kernel_stack_top

section .text

extern OsInitialize
global x64Entry

x64Entry:
    cli
    mov rsp, kernel_stack_top
    jmp OsInitialize
