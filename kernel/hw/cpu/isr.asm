struc InterruptFrame
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

section .data

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

R0_CODE_SEL equ 1*8
R0_DATA_SEL equ 2*8

; %1 int_no, %2 has_error
%macro GENERATE_ISR 2
global _Isr%1
_Isr%1:
    %if %2 == NO_ERROR
        ; Push an error code for stack consistency
        push 0
    %endif

    ; + 16 is cs in the interrupt frame
    ; before the rest has been set up
    cmp qword [rsp + 16], R0_CODE_SEL
    je .kentry

    ; Only swap GS if coming from user mode
    swapgs

.kentry:
    ; Generate rest of interrupt frame
    push rbp
    PUSH_GPR

    ; Call generic handler
    mov rcx, rsp
    mov rdx, %1
    sub rsp, 32 ; shadow space
    cld
    call IsrCommon
    add rsp, 32

    ; Free interrupt frame
    POP_GPR
    pop rbp

    cmp qword [rsp + 16], R0_CODE_SEL
    je .kexit

    swapgs

.kexit:
    ; Free error code
    add rsp, 8

    iretq
%endmacro

; %1 first, %2 last, %3 has_error
%macro GENERATE_ISRS 3
%assign i %1
    %rep %2 - %1+1
        GENERATE_ISR i, %3
        %assign i i+1
    %endrep
%endmacro

global _IsrSpurious
_IsrSpurious:
    inc qword [rel spurious_irqs]
    ; TODO - check if spurious_irqs is really high
    ; and issue a warning or mask the interrupt entirely
    iretq

GENERATE_ISRS 0, 7, 0
GENERATE_ISR 8, 1
GENERATE_ISR 9, 0
GENERATE_ISRS 10, 14, 1
GENERATE_ISR 15, 0
GENERATE_ISR 16, 0
GENERATE_ISR 17, 1
GENERATE_ISR 18, 0
GENERATE_ISR 19, 0
GENERATE_ISR 20, 0
GENERATE_ISR 21, 1
GENERATE_ISRS 22, 29, 0
GENERATE_ISR 30, 1
GENERATE_ISR 31, 0

GENERATE_ISRS 32, 255, 0
