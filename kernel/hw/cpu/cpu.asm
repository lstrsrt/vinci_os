extern kernel_stack_top: qword
extern OsInitialize: proc

.code

; TODO - move entry somewhere else

;
; NO_RETURN void x64Entry()
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
; Loads new code and data segments
;
; dx = New data selector
; cx = New code selector
;
ReloadSegments proc
    ; Loading data segments can be done with simple movs
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx
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
; void Ring3Function()
;
; Sample function to demonstrate entering ring 3
;
Ring3Function proc
    mov rax, 0adadadadadadadadh
    mov rbx, rax
inf_loop:
    jmp inf_loop
    ret
Ring3Function endp

;
; void EnterUserMode()
;
; Transfers control to ring 3 via sysret.
; CS and SS are changed to user segments.
;
; rcx = User code address
; rdx = User stack address
;
EnterUserMode proc
    cli
    ; mov rcx, 7ff7f0000000h ; user_page_va
    mov r11, 200h ; 300h
    mov rsp, rdx ; user_stack_va
    sysretq
EnterUserMode endp

x64SysCall proc
    sysretq
x64SysCall endp

end
