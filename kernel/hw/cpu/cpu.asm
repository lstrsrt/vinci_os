extern kernel_stack_top: qword
extern OsInitialize: proc
extern x64SyscallCxx: proc

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
    mov rdi, 123
    mov rax, 1
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
    swapgs            ; Switch to kernel gs from KERNEL_GS_BASE MSR

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

end
