struc Context
    .rax         resq 1
    .rcx         resq 1
    .rdx         resq 1
    .rbx         resq 1
    .rsi         resq 1
    .rdi         resq 1
    .r8          resq 1
    .r9          resq 1
    .r10         resq 1
    .r11         resq 1
    .r12         resq 1
    .r13         resq 1
    .r14         resq 1
    .r15         resq 1
    .rbp         resq 1
    .error_code  resq 1
    .rip         resq 1
    .cs          resq 1
    .rflags      resq 1
    .rsp         resq 1
    .ss          resq 1
endstruc

%define sysretq o64 sysret

section .data
extern kernel_stack_base

section .text
extern OsInitialize
extern SyscallCxx
extern _ExitThread

global x64Entry
global ReloadSegments
global LoadTr
global UserThreadEntry
global Ring3Function
global SyscallEntry
global LoadContext
global SwitchContext

;
; NO_RETURN void x64Entry(LoaderBlock*)
;
; Main x64 architecture entry point
;
; rcx = Loader block (used by OsInitialize)
;
x64Entry:
    cli
    mov rsp, [rel kernel_stack_base]
    sub rsp, 40
    jmp OsInitialize

;
; void ReloadSegments(u16 code_selector, u16 data_selector)
;
; Loads new code and data segments.
; FS and GS are not changed!
;
; dx = New data selector
; cx = New code selector
;
ReloadSegments:
    ; Loading data segments can be done with simple movs
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov ss, dx

    ; Set rdx to the label address and do a far return
    ; This will reload cs with whatever is in rcx
    ; (as long as the corresponding GDT entry is valid)
    movzx rcx, cx
    lea rdx, [rel .exit]
    push rcx
    push rdx
    retfq
.exit:
    ret

;
; void LoadTr(u16 offset)
;
; Wraps the LTR instruction which changes the active task register
;
; cx = New task register offset
;
LoadTr:
    ltr cx
    ret

;
; NO_RETURN int UserThreadEntry(u64 arg, void* function)
;
; User mode thread entry point.
;
; rcx = User arg
; rdx = User function
;
UserThreadEntry:
    sub rsp, 40
    call rdx
    mov edi, eax
    mov eax, 3
    syscall ; ExitThread
    ud2

;
; NO_RETURN int Ring3Function(u64 arg)
;
; Sample function to demonstrate entering ring 3.
;
; rcx = Limit
;
Ring3Function:
    mov rdx, rcx
.loop:
    mov eax, 2
    mov edi, 1000
    syscall ; Delay
    dec edx
    jnz .loop
    mov eax, 3
    ret

;
; u64 SyscallEntry()
;
; Main syscall entry routine.
; Loads the kernel stack, constructs a SyscallFrame and calls SyscallCxx
; which selects and runs the requested system service.
;
; Result is returned in rax.
;
SyscallEntry:
    swapgs             ; Switch to kernel gs (at KERNEL_GS_BASE MSR)

    mov gs:[48], rsp   ; core->user_stack = rsp
    mov rsp, gs:[40]   ; rsp = core->kernel_stack_top
    push qword gs:[48] ; push core->user_stack

    sti

    push r11 ; rflags
    push r10
    push r9
    push r8
    push rcx
    push rdx
    push rax ; int_no
    push rdi
    push rsi
    push r11

    mov rcx, rsp  ; frame
    mov rdx, rax  ; int_no
    sub rsp, 40   ; shadow space
    call SyscallCxx
    add rsp, 40

    mov rdx, [rsp + 40]
    shr rdx, 47
    jnz .not_canonical

    pop r11
    pop rsi
    pop rdi
    add rsp, 8 ; Don't restore rax since it holds the return value
    pop rdx
    pop rcx
    pop r8
    pop r9
    pop r10
    pop r11

    cli
    pop rsp

    ; Back to user mode
    swapgs
    sysretq

.not_canonical:
    sub rsp, 40
    mov ecx, -1
    call _ExitThread
    ud2

;
; NO_RETURN void LoadContext(Context* ctx)
;
; Loads a new thread context.
;
; rcx = Thread context
;
LoadContext:
    cli

    push [rcx + Context.ss]
    push [rcx + Context.rsp]
    push [rcx + Context.rflags]
    push [rcx + Context.cs]
    push [rcx + Context.rip]

    mov rbp, [rcx + Context.rbp]
    mov rax, [rcx + Context.rax]
    mov rbx, [rcx + Context.rbx]
    mov rdx, [rcx + Context.rdx]
    mov rdi, [rcx + Context.rdi]
    mov rsi, [rcx + Context.rsi]
    mov r8, [rcx + Context.r8]
    mov r9, [rcx + Context.r9]
    mov r10, [rcx + Context.r10]
    mov r11, [rcx + Context.r11]
    mov r12, [rcx + Context.r12]
    mov r13, [rcx + Context.r13]
    mov r14, [rcx + Context.r14]
    mov r15, [rcx + Context.r15]

    test byte [rcx + Context.cs], 3
    je .kernel
    swapgs
.kernel:
    mov rcx, [rcx + Context.rcx]
    iretq

;
; NO_RETURN void SwitchContext(Context* prev, Context* next)
;
; Stores the context of the calling thread in `prev` and loads `next`.
;
; rcx = Previous context
; rdx = Next context
;
SwitchContext:
    mov [rcx + Context.rbp], rbp
    mov [rcx + Context.rax], rax
    mov [rcx + Context.rbx], rbx
    mov [rcx + Context.rcx], rcx
    mov [rcx + Context.rdx], rdx
    mov [rcx + Context.rdi], rdi
    mov [rcx + Context.rsi], rsi
    mov [rcx + Context.r8], r8
    mov [rcx + Context.r9], r9
    mov [rcx + Context.r10], r10
    mov [rcx + Context.r11], r11
    mov [rcx + Context.r12], r12
    mov [rcx + Context.r13], r13
    mov [rcx + Context.r14], r14
    mov [rcx + Context.r15], r15

    pushfq
    pop r9
    mov [rcx + Context.rflags], r9

    ; Return address
    pop r9
    mov [rcx + Context.rip], r9

    mov [rcx + Context.rsp], rsp

    mov [rcx + Context.ss], ss
    mov eax, cs
    mov [rcx + Context.cs], rax ; zero-fill

    mov rcx, rdx
    jmp LoadContext
