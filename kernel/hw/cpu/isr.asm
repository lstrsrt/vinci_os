section .bss

extern spurious_irqs

section .text

extern IsrCommon

%macro PUSH_GPR 0
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
%endmacro

%macro POP_GPR 0
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
%endmacro

HAS_ERROR equ 1
NO_ERROR  equ 0

; int_no, has_error
%macro GENERATE_ISR 2
global _Isr%1
_Isr%1:
    ; Clear DF before calling into any C++ code
    cld

    %if %2 == NO_ERROR
        ; Push an error code for stack consistency
        push 0
    %endif

    ; Generate rest of interrupt frame
    push rbp
    PUSH_GPR

    ; Call generic handler
    mov rcx, rsp
    mov rdx, %1
    sub rsp, 32 ; shadow space
    call IsrCommon
    add rsp, 32

    ; Free interrupt frame
    POP_GPR
    pop rbp

    ; Free error code
    add rsp, 8

    iretq
%endmacro

; first, last, has_error
%macro GENERATE_ISRS 3
%assign i %1
    %rep %2 - %1 + 1
        GENERATE_ISR i, %3
        %assign i i+1
    %endrep
%endmacro

global _IsrSpurious
_IsrSpurious:
    cld
    inc qword [spurious_irqs]
    ; TODO - check if spurious_irqs is really high
    ; and issue a warning or mask the interrupt entirely
    iretq

GENERATE_ISR 0, 0
GENERATE_ISR 1, 0
GENERATE_ISR 2, 0
GENERATE_ISR 3, 0
GENERATE_ISR 4, 0
GENERATE_ISR 5, 0
GENERATE_ISR 6, 0
GENERATE_ISR 7, 0
GENERATE_ISR 8, 1
GENERATE_ISR 9, 0
GENERATE_ISR 10, 1
GENERATE_ISR 11, 1
GENERATE_ISR 12, 1
GENERATE_ISR 13, 1
GENERATE_ISR 14, 1
GENERATE_ISR 15, 0
GENERATE_ISR 16, 0
GENERATE_ISR 17, 1
GENERATE_ISR 18, 0
GENERATE_ISR 19, 0
GENERATE_ISR 20, 0
GENERATE_ISR 21, 1
GENERATE_ISR 22, 0
GENERATE_ISR 23, 0
GENERATE_ISR 24, 0
GENERATE_ISR 25, 0
GENERATE_ISR 26, 0
GENERATE_ISR 27, 0
GENERATE_ISR 28, 0
GENERATE_ISR 29, 0
GENERATE_ISR 30, 1
GENERATE_ISR 31, 0

GENERATE_ISRS 32, 255, 0
