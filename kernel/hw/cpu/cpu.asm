extern kernel_stack_top: qword
extern OsInitialize: proc
extern x64SyscallCxx: proc

.data

InterruptFrame struct
    _rax qword ?
    _rcx qword ?
    _rdx qword ?
    _rbx qword ?
    _rsi qword ?
    _rdi qword ?
    _r8 qword ?
    _r9 qword ?
    _r10 qword ?
    _r11 qword ?
    _r12 qword ?
    _r13 qword ?
    _r14 qword ?
    _r15 qword ?
    _fs qword ?
    _gs qword ?
    _rbp qword ?
    _unused qword ?
    _rip qword ?
    _cs qword ?
    _rflags qword ?
    _rsp qword ?
    _ss qword ?
InterruptFrame ends

.code

;
; NO_RETURN void x64Entry(LoaderBlock*)
;
; Main x64 architecture entry point
;
; rcx = Loader block (used by OsInitialize)
;
x64Entry proc
    cli
    mov rsp, kernel_stack_top
    sub rsp, 32 ; shadow space
    jmp OsInitialize
x64Entry endp

;
; void ReloadSegments(u16 code_selector, u16 data_selector)
;
; Loads new code and data segments.
; FS and GS are not changed!
;
; dx = New data selector
; cx = New code selector
;
ReloadSegments proc
    ; Loading data segments can be done with simple movs
    mov ds, dx
    mov es, dx
    mov ss, dx

    ; Set rdx to the label address and do a far return
    ; This will reload cs with whatever is in rcx
    ; (as long as the corresponding GDT entry is valid)
    movzx rcx, cx
    lea rdx, exit
    push rcx
    push rdx
    retfq

exit:
    ret
ReloadSegments endp

;
; void LoadTr(u16 offset)
;
; Wraps the LTR instruction which changes the active task register
;
; cx = New task register offset
;
LoadTr proc
    ltr cx
    ret
LoadTr endp

;
; NO_RETURN void Ring3Function()
;
; Sample function to demonstrate entering ring 3.
; Never returns!
;
Ring3Function proc
    mov rdi, 1000
    mov rax, 2
    syscall
inf_loop:
    jmp inf_loop
    ret
Ring3Function endp

;
; void EnterUserMode(vaddr_t code, vaddr_t stack)
;
; Transfers control to ring 3 via sysret.
;
; rcx = User code address
; rdx = User stack address
;
EnterUserMode proc
    mov r11, 202h ; new rflags
    mov rsp, rdx
    ; sysret jumps to rcx, which should be a canonical VA
    ; mapped with the user mode bit set.
    sysretq
EnterUserMode endp

;
; u64 x64Syscall()
;
; Main syscall entry routine.
; Loads the kernel stack, constructs a SyscallFrame and calls x64SyscallCxx
; which selects and runs the requested system service.
;
; Result is returned in rax.
;
x64Syscall proc
    swapgs            ; Switch to kernel gs (at KERNEL_GS_BASE MSR)

    mov gs:[8], rsp   ; Store user stack

    ; TODO - currently this is just kernel_stack_top,
    ; have to store the kernel stack location first...
    mov rsp, gs:[0]   ; Switch to kernel stack

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
    sysretq
x64Syscall endp

;
; void SetRegs()
;
SetRegs proc
    mov rbx, rcx
    mov rsi, rcx
    mov rdi, rcx
    mov r12, rcx
    mov r13, rcx
    mov r14, rcx
    mov r15, rcx
    ret
SetRegs endp

;
; void SaveContext(InterruptFrame* ctx)
;
SaveContext proc
    mov [rcx + InterruptFrame._rsp], rsp
    mov [rcx + InterruptFrame._rbp], rbp
    mov [rcx + InterruptFrame._rbx], rbx
    mov [rcx + InterruptFrame._rdi], rdi
    mov [rcx + InterruptFrame._rsi], rsi
    mov [rcx + InterruptFrame._r12], r12
    mov [rcx + InterruptFrame._r13], r13
    mov [rcx + InterruptFrame._r14], r14
    mov [rcx + InterruptFrame._r15], r15

    pushfq
    pop rax
    mov [rcx + InterruptFrame._rflags], rax

    ; Get the return address from the stack
    pop rdx
    mov [rcx + InterruptFrame._rip], rdx
    mov [rcx + InterruptFrame._rsp], rsp

    jmp rdx
SaveContext endp

;
; void LoadContext(InterruptFrame* ctx)
;
LoadContext proc
    mov rsp, [rcx + InterruptFrame._rsp]
    mov rbp, [rcx + InterruptFrame._rbp]
    mov rbx, [rcx + InterruptFrame._rbx]
    mov rdi, [rcx + InterruptFrame._rdi]
    mov rsi, [rcx + InterruptFrame._rsi]
    mov r12, [rcx + InterruptFrame._r12]
    mov r13, [rcx + InterruptFrame._r13]
    mov r14, [rcx + InterruptFrame._r14]
    mov r15, [rcx + InterruptFrame._r15]

    mov r8, [rcx + InterruptFrame._rflags]
    push r8
    popfq

    mov r9, [rcx + InterruptFrame._rip]
    jmp r9
LoadContext endp

end
