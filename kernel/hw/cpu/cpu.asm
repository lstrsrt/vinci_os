extern kernel_stack_top: qword
extern OsInitialize: proc

.code

;
; NO_RETURN void x64Entry(LoaderBlock*)
;
; Main x64 architecture entry point
;
; rcx = Loader block (used by OsInitialize)
;
x64Entry proc
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
    mov rax, 0adadadadadadadadh
    mov rbx, rax
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

x64SysCall proc
    sysretq
x64SysCall endp

end
