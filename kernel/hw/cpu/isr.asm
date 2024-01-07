extern IsrCommon: proc
extern spurious_irqs: qword

PUSH_GPR macro
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbx
    push rdx
    push rcx
    push rax
endm

POP_GPR macro
    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
endm

HAS_ERROR equ 1
NO_ERROR  equ 0

GENERATE_ISR macro int_no, has_error
_Isr&int_no& proc
    ; Clear DF before calling into any C++ code
    cld

    if has_error eq NO_ERROR
        ; Push an error code for stack consistency
        push 0
    endif

    ; Generate rest of interrupt frame
    push rbp
    PUSH_GPR

    ; Call generic handler
    mov rcx, rsp
    mov rdx, int_no
    sub rsp, 32 ; shadow space
    call IsrCommon
    add rsp, 32

    ; Free interrupt frame
    POP_GPR
    pop rbp

    ; Free error code
    add rsp, 8

    iretq
_Isr&int_no& endp
endm

GENERATE_ISRS macro first_int, last_int, has_error
num = first_int
while num le last_int
    GENERATE_ISR %num, %has_error
    num = num + 1
endm
endm

.code

_IsrSpurious proc
    cld
    inc spurious_irqs
    ; TODO - check if spurious_irqs is really high
    ; and issue a warning or mask the interrupt entirely
    iretq
_IsrSpurious endp

GENERATE_ISR 0, NO_ERROR
GENERATE_ISR 1, NO_ERROR
GENERATE_ISR 2, NO_ERROR
GENERATE_ISR 3, NO_ERROR
GENERATE_ISR 4, NO_ERROR
GENERATE_ISR 5, NO_ERROR
GENERATE_ISR 6, NO_ERROR
GENERATE_ISR 7, NO_ERROR
GENERATE_ISR 8, HAS_ERROR
GENERATE_ISR 9, NO_ERROR
GENERATE_ISR 10, HAS_ERROR
GENERATE_ISR 11, HAS_ERROR
GENERATE_ISR 12, HAS_ERROR
GENERATE_ISR 13, HAS_ERROR
GENERATE_ISR 14, HAS_ERROR
GENERATE_ISR 15, NO_ERROR
GENERATE_ISR 16, NO_ERROR
GENERATE_ISR 17, HAS_ERROR
GENERATE_ISR 18, NO_ERROR
GENERATE_ISR 19, NO_ERROR
GENERATE_ISR 20, NO_ERROR
GENERATE_ISR 21, HAS_ERROR
GENERATE_ISR 22, NO_ERROR
GENERATE_ISR 23, NO_ERROR
GENERATE_ISR 24, NO_ERROR
GENERATE_ISR 25, NO_ERROR
GENERATE_ISR 26, NO_ERROR
GENERATE_ISR 27, NO_ERROR
GENERATE_ISR 28, NO_ERROR
GENERATE_ISR 29, NO_ERROR
GENERATE_ISR 30, HAS_ERROR
GENERATE_ISR 31, NO_ERROR

GENERATE_ISRS 32, 255, NO_ERROR

end
