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
extern kernel_stack_top

section .text
extern OsInitialize
extern SyscallCxx

global x64Entry
global ReloadSegments
global LoadTr
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
    mov rsp, [rel kernel_stack_top]
    sub rsp, 32
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
; NO_RETURN void Ring3Function()
;
; Sample function to demonstrate entering ring 3.
;
Ring3Function:
    mov edi, 1000 ; number
    mov ecx, 5
.loop:
    mov eax, 2 ; sys_no
    syscall
    dec ecx
    jnz .loop
    ; jmp .loop

    ; ExitThread
    mov eax, 3
    mov edi, 5 ; exit_code
    syscall ; ExitThread
    ret

;
; u64 SyscallEntry()
;
; Main syscall entry routine.
; Loads the kernel stack, constructs a SyscallFrame and calls x64SyscallCxx
; which selects and runs the requested system service.
;
; Result is returned in rax.
;
SyscallEntry:
    swapgs           ; Switch to kernel gs (at KERNEL_GS_BASE MSR)

    mov gs:[48], rsp ; core->user_stack = rsp
    mov rsp, gs:[40] ; rsp = core->kernel_stack

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

    mov rcx, rsp  ; SyscallFrame
    mov rdx, rax  ; int_no
    sub rsp, 32   ; shadow space
    call SyscallCxx
    add rsp, 32

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

    mov gs:[40], rsp
    mov rsp, gs:[48]

    ; Back to user mode
    swapgs
    sysretq

;
; NO_RETURN void LoadContext(Context* ctx, uptr_t user_stack)
;
; Loads a new thread context.
;
; rcx = Thread context
; rdx = Thread user stack if applicable
;
LoadContext:
    mov rbp, [rcx + Context.rbp]
    mov rbx, [rcx + Context.rbx]
    mov rdi, [rcx + Context.rdi]
    mov rsi, [rcx + Context.rsi]
    mov r12, [rcx + Context.r12]
    mov r13, [rcx + Context.r13]
    mov r14, [rcx + Context.r14]
    mov r15, [rcx + Context.r15]

    mov r11, [rcx + Context.rflags]

    mov rsp, [rcx + Context.rsp]
    mov rcx, [rcx + Context.rip]

    ; Was a user stack provided?
    test rdx, rdx
    jz .ring0

    ; Is RIP in ring 0 even though it's a user thread?
    ; This can happen when a syscall was interrupted
    mov r9, 7fffffff0000h
    cmp rcx, r9
    jb .ring3

.ring0:
    ; We're staying in ring 0 - flags have to be loaded manually.
    push r11
    popfq

    ; Go to the new thread IP, no more work required
    jmp rcx

.ring3:
    mov rsp, rdx
    ; User mode needs to change selectors, so simulate a system call return.
    ; Flags are loaded automatically from r11.
    swapgs
    sysretq

;
; NO_RETURN void SwitchContext(Context* prev, Context* next, uptr_t user_stack)
;
; Stores the context of the calling thread in `prev` and loads `next`.
;
; rcx = Previous context
; rdx = Next context
; r8  = Thread user stack if applicable
;
SwitchContext:
    mov [rcx + Context.rbp], rbp
    mov [rcx + Context.rbx], rbx
    mov [rcx + Context.rdi], rdi
    mov [rcx + Context.rsi], rsi
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

    mov rcx, rdx
    mov rdx, r8
    jmp LoadContext
