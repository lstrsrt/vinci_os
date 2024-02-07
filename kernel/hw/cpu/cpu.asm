extern kernel_stack_top
extern OsInitialize
extern x64SyscallCxx

struc Context
    ._rax    resq 1
    ._rcx    resq 1
    ._rdx    resq 1
    ._rbx    resq 1
    ._rsi    resq 1
    ._rdi    resq 1
    ._r8     resq 1
    ._r9     resq 1
    ._r10    resq 1
    ._r11    resq 1
    ._r12    resq 1
    ._r13    resq 1
    ._r14    resq 1
    ._r15    resq 1
    ._rbp    resq 1
    ._rsp    resq 1
    ._rip    resq 1
    ._rflags resq 1
endstruc

section .text

global x64Entry
global ReloadSegments
global LoadTr
global x64Syscall
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
    mov es, dx
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
; Never returns!
;
Ring3Function:
    mov rdi, 1000
    mov rax, 2
    syscall
.inf_loop:
    jmp .inf_loop
    ret

;
; void EnterUserMode(vaddr_t code, vaddr_t stack)
;
; Transfers control to ring 3 via sysret.
;
; rcx = User code address
; rdx = User stack address
;
EnterUserMode:
    mov r11, 202h ; new rflags
    mov rsp, rdx
    sysret
    ; sysret jumps to rcx, which should be a canonical VA
    ; mapped with the user mode bit set.

;
; u64 x64Syscall()
;
; Main syscall entry routine.
; Loads the kernel stack, constructs a SyscallFrame and calls x64SyscallCxx
; which selects and runs the requested system service.
;
; Result is returned in rax.
;
x64Syscall:
    swapgs            ; Switch to kernel gs (at KERNEL_GS_BASE MSR)

    ; mov gs xxx, rsp   ; Store user stack

    ; mov rsp, gs xxx   ; Switch to kernel stack

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
    call x64SyscallCxx
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

    ; Back to user mode
    swapgs
    sysret

;
; NO_RETURN void LoadContext(Context* ctx)
;
; Loads a new thread context.
;
; rcx = Context
;
LoadContext:
    mov rsp, [rcx + Context._rsp]
    mov rbp, [rcx + Context._rbp]
    mov rbx, [rcx + Context._rbx]
    mov rdi, [rcx + Context._rdi]
    mov rsi, [rcx + Context._rsi]
    mov r12, [rcx + Context._r12]
    mov r13, [rcx + Context._r13]
    mov r14, [rcx + Context._r14]
    mov r15, [rcx + Context._r15]

    mov r8, [rcx + Context._rflags]
    push r8
    popfq

    mov r9, [rcx + Context._rip]
    jmp r9

;
; NO_RETURN void SwitchContext(Context* prev, Context* next)
;
; Stores the context of the calling thread in `prev` and loads `next`.
;
; rcx = Previous context
; rdx = Next context
;
SwitchContext:
    mov [rcx + Context._rbp], rbp
    mov [rcx + Context._rbx], rbx
    mov [rcx + Context._rdi], rdi
    mov [rcx + Context._rsi], rsi
    mov [rcx + Context._r12], r12
    mov [rcx + Context._r13], r13
    mov [rcx + Context._r14], r14
    mov [rcx + Context._r15], r15

    pushfq
    pop r8
    mov [rcx + Context._rflags], r8

    ; Return address
    pop r8
    mov [rcx + Context._rip], r8

    mov [rcx + Context._rsp], rsp

    mov rcx, rdx
    jmp LoadContext
