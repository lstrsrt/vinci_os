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

GENERATE_ISR macro has_errcode, int_no
_Isr&int_no& proc
    ; Clear DF before calling into any C++ code
    cld

    IF has_errcode EQ NO_ERROR
    ; Push an error code for stack consistency
    push 0
    ENDIF

    ; Generate rest of interrupt frame
    push rbp
    mov rbp, rsp
    push gs
    push fs
    PUSH_GPR

    ; Call generic handler
    mov rcx, rsp
    mov rdx, int_no
    sub rsp, 32 ; shadow space
    call IsrCommon
    add rsp, 32

    ; Free interrupt frame
    POP_GPR
    add rsp, 16 ; fs and gs
    pop rbp

    ; Free error code
    add rsp, 8

    iretq
_Isr&int_no& endp
endm

.code

_IsrSpurious proc
    cld
    inc spurious_irqs
    ; TODO - check if spurious_irqs is really high
    ; and issue a warning or mask the interrupt entirely
    iretq
_IsrSpurious endp

; TODO - bad design, remove asap
; we should just generate all 256 routines via macro
_IsrUnexpected proc
    cld

    push rbp
    mov rbp, rsp
    push gs
    push fs
    PUSH_GPR

    mov rcx, rsp
    mov rdx, 666 ; Has to be > irq_base + irq_count
    sub rsp, 32
    call IsrCommon
    add rsp, 32

    POP_GPR
    add rsp, 16
    pop rbp

    iretq
_IsrUnexpected endp

GENERATE_ISR NO_ERROR,  0
GENERATE_ISR NO_ERROR,  1
GENERATE_ISR NO_ERROR,  2
GENERATE_ISR NO_ERROR,  3
GENERATE_ISR NO_ERROR,  4
GENERATE_ISR NO_ERROR,  5
GENERATE_ISR NO_ERROR,  6
GENERATE_ISR NO_ERROR,  7
GENERATE_ISR HAS_ERROR, 8
GENERATE_ISR NO_ERROR,  9
GENERATE_ISR HAS_ERROR, 10
GENERATE_ISR HAS_ERROR, 11
GENERATE_ISR HAS_ERROR, 12
GENERATE_ISR HAS_ERROR, 13
GENERATE_ISR HAS_ERROR, 14
GENERATE_ISR NO_ERROR,  15
GENERATE_ISR NO_ERROR,  16
GENERATE_ISR HAS_ERROR, 17
GENERATE_ISR NO_ERROR,  18
GENERATE_ISR NO_ERROR,  19
GENERATE_ISR NO_ERROR,  20
GENERATE_ISR HAS_ERROR, 21
GENERATE_ISR NO_ERROR,  22
GENERATE_ISR NO_ERROR,  23
GENERATE_ISR NO_ERROR,  24
GENERATE_ISR NO_ERROR,  25
GENERATE_ISR NO_ERROR,  26
GENERATE_ISR NO_ERROR,  27
GENERATE_ISR NO_ERROR,  28
GENERATE_ISR NO_ERROR,  29
GENERATE_ISR HAS_ERROR, 30
GENERATE_ISR NO_ERROR,  31

GENERATE_ISR NO_ERROR, 32
GENERATE_ISR NO_ERROR, 33
GENERATE_ISR NO_ERROR, 34
GENERATE_ISR NO_ERROR, 35
GENERATE_ISR NO_ERROR, 36
GENERATE_ISR NO_ERROR, 37
GENERATE_ISR NO_ERROR, 38
GENERATE_ISR NO_ERROR, 39
GENERATE_ISR NO_ERROR, 40
GENERATE_ISR NO_ERROR, 41
GENERATE_ISR NO_ERROR, 42
GENERATE_ISR NO_ERROR, 43
GENERATE_ISR NO_ERROR, 44
GENERATE_ISR NO_ERROR, 45
GENERATE_ISR NO_ERROR, 46
GENERATE_ISR NO_ERROR, 47

end
