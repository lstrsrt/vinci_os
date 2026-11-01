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

; %1 int_no, %2 has_error
%macro GENERATE_ISR 2
global _Isr%1
_Isr%1:
    %if %2 == NO_ERROR
        ; Push an error code for stack consistency
        push 0
    %endif

    ; + 16 is CS in the interrupt frame
    ; before the rest has been set up
    test byte [rsp + 16], 3
    je .kentry

    ; Only swap GS if coming from user mode
    swapgs

.kentry:
    ; Generate rest of interrupt frame
    push rbp
    PUSH_GPR

    ; Call generic handler
    mov rcx, rsp ; stack
    mov rdx, %1  ; int no
    sub rsp, 40  ; shadow space
    push qword 2 ; clear rflags
    popfq
    call IsrCommon
    add rsp, 40

    ; Free interrupt frame
    POP_GPR
    pop rbp

    test byte [rsp + 16], 3
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

; This macro is used for interrupts that ignore RFLAG::IF,
; currently #NMI, #DB and #MC. If e.g. a syscall gets interrupted
; by one of these before it can swapgs, our CS=3 test would fail
; and we would run with a user GS.
; This macro reads the GS base directly instead.
;
; %1 int_no
%macro GENERATE_PARANOID_ISR 1
global _Isr%1
_Isr%1:
    ; Fake error code
    push 0
    ; Generate rest of interrupt frame
    push rbp
    PUSH_GPR

    ; Read GS base
    mov ecx, 0xC0000101
    rdmsr
    xor ebx, ebx
    test edx, edx
    js .kentry ; signed = kernel

    ; Swap GS if base is usermode
    swapgs
    mov ebx, 1

.kentry:
    ; Call generic handler
    mov rcx, rsp ; stack
    mov rdx, %1  ; int no
    sub rsp, 40  ; shadow space
    push qword 2 ; clear rflags
    popfq
    call IsrCommon
    add rsp, 40

    test ebx, ebx
    jz .kexit
    swapgs

.kexit:
    ; Free interrupt frame
    POP_GPR
    pop rbp

    ; Free error code
    add rsp, 8
    iretq
%endmacro

global _IsrSpurious
_IsrSpurious:
    inc qword [rel spurious_irqs]
    ; TODO - check if spurious_irqs is really high
    ; and issue a warning or mask the interrupt entirely
    iretq

GENERATE_ISR 0, NO_ERROR
GENERATE_PARANOID_ISR 1
GENERATE_PARANOID_ISR 2
GENERATE_ISRS 3, 7, NO_ERROR
GENERATE_ISR 8, HAS_ERROR
GENERATE_ISR 9, NO_ERROR
GENERATE_ISRS 10, 14, HAS_ERROR
GENERATE_ISRS 15, 16, NO_ERROR
GENERATE_ISR 17, HAS_ERROR
GENERATE_PARANOID_ISR 18
GENERATE_ISRS 19, 20, NO_ERROR
GENERATE_ISR 21, HAS_ERROR
GENERATE_ISRS 22, 28, NO_ERROR
GENERATE_ISRS 29, 30, HAS_ERROR
GENERATE_ISR 31, NO_ERROR
GENERATE_ISRS 32, 255, NO_ERROR
