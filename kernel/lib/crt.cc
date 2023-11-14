#include "base.h"

/* CRT symbols for compatibility with LINK.exe. */

EXTERN_C_START

void __chkstk()
{

}

__declspec(selectany) int _fltused = 1;

//RAW void _allshl()
//{
//    __asm {
//        cmp cl, 0x40
//        jae RETZERO
//        cmp cl, 0x20
//        jae MORE32
//        shld edx, eax, cl
//        shl eax, cl
//        ret
//    MORE32:
//        mov edx, eax
//        xor eax, eax
//        and cl, 0x1f
//        shl edx, cl
//        ret
//    RETZERO:
//        xor eax, eax
//        xor edx, edx
//        ret
//    }
//}
//
//RAW void _aullshr()
//{
//    __asm {
//        cmp cl, 0x40
//        jae RETZERO
//        cmp cl, 0x20
//        jae MORE32
//        shrd eax, edx, cl
//        shr edx, cl
//        ret
//    MORE32:
//        mov eax, edx
//        xor edx, edx
//        and cl, 0x1f
//        shr eax, cl
//        ret
//    RETZERO:
//        xor eax, eax
//        xor edx, edx
//        ret
//    }
//}
//
//RAW void _allmul()
//{
//#define ALO  [esp + 4]
//#define AHI  [esp + 8]
//#define BLO  [esp + 12]
//#define BHI  [esp + 16]
//    __asm {
//        mov eax, AHI
//        mov ecx, BHI
//        or ecx, eax 
//        mov ecx, BLO
//        jnz hard
//        mov eax, ALO
//        mul ecx
//        ret 16
//    hard:
//        push ebx
//#define A2LO  [esp + 8]     
//#define A2HI  [esp + 12]    
//#define B2LO  [esp + 16]    
//#define B2HI  [esp + 20]    
//        mul ecx         
//        mov ebx, eax    
//        mov eax, A2LO
//        mul dword ptr B2HI
//        add ebx, eax      
//        mov eax, A2LO  
//        mul ecx        
//        add edx, ebx   
//        pop ebx
//        ret 16
//    }
//}
//
//RAW void _aulldiv()
//{
//#define DVNDLO  [esp + 12]    
//#define DVNDHI  [esp + 16]    
//#define DVSRLO  [esp + 20]    
//#define DVSRHI  [esp + 24]
//    __asm {
//        push ebx
//        push esi
//        mov eax, DVSRHI
//        or eax, eax
//        jnz L1        
//        mov ecx, DVSRLO
//        mov eax, DVNDHI
//        xor edx, edx
//        div ecx         
//        mov ebx, eax    
//        mov eax, DVNDLO 
//        div ecx         
//        mov edx, ebx    
//        jmp L2        
//    L1:
//        mov ecx, eax   
//        mov ebx, DVSRLO
//        mov edx, DVNDHI
//        mov eax, DVNDLO
//    L3:
//        shr ecx, 1  
//        rcr ebx, 1
//        shr edx, 1  
//        rcr eax, 1
//        or ecx, ecx
//        jnz L3      
//        div ebx     
//        mov esi, eax
//        mul dword ptr DVSRHI
//        mov ecx, eax
//        mov eax, DVSRLO
//        mul esi       
//        add edx, ecx  
//        jc L4      
//        cmp edx, DVNDHI
//        ja L4       
//        jb L5       
//        cmp eax, DVNDLO
//        jbe L5        
//    L4:
//        dec esi          
//    L5:
//        xor edx, edx     
//        mov eax, esi
//    L2:
//        pop esi
//        pop ebx
//        ret 16
//    }
//#undef DVNDLO
//#undef DVNDHI
//#undef DVSRLO
//#undef DVSRHI
//}
//
//RAW void _aullrem()
//{
//#define DVNDLO  [esp + 8]    
//#define DVNDHI  [esp + 12]   
//#define DVSRLO  [esp + 16]   
//#define DVSRHI  [esp + 20]
//    __asm {
//        push ebx
//        mov eax, DVSRHI 
//        or eax, eax
//        jnz L1    
//        mov ecx, DVSRLO 
//        mov eax, DVNDHI 
//        xor edx, edx
//        div ecx         
//        mov eax, DVNDLO 
//        div ecx         
//        mov eax, edx    
//        xor edx, edx
//        jmp L2     
//    L1:
//        mov ecx, eax        
//        mov ebx, DVSRLO
//        mov edx, DVNDHI 
//        mov eax, DVNDLO
//    L3:
//        shr ecx, 1          
//        rcr ebx, 1
//        shr edx, 1        
//        rcr eax, 1
//        or ecx, ecx
//        jnz L3  
//        div ebx
//        mov ecx, eax         
//        mul dword ptr DVSRHI
//        xchg ecx, eax         
//        mul dword ptr DVSRLO
//        add edx, ecx         
//        jc L4
//        cmp edx, DVNDHI 
//        ja L4      
//        jb L5        
//        cmp eax, DVNDLO 
//        jbe L5     
//    L4:
//        sub eax, DVSRLO 
//        sbb edx, DVSRHI
//    L5:
//        sub eax, DVNDLO
//        sbb edx, DVNDHI
//        neg edx            
//        neg eax
//        sbb edx, 0
//    L2:
//        pop ebx
//        ret 16
//    }
//}
//
//
//
//RAW void _ftol2()
//{
//    __asm {
//        push        ebp
//        mov         ebp, esp
//        sub         esp, 20h
//        and         esp, 0xfffffff0
//        fld         st(0)
//        fst         dword ptr[esp + 0x18]
//        fistp       qword ptr[esp + 0x10]
//        fild        qword ptr[esp + 0x10]
//        mov         edx, dword ptr[esp + 0x18]
//        mov         eax, dword ptr[esp + 0x10]
//        test        eax, eax
//        je          integer_qnan_or_zero
//    arg_is_not_integer_qnan:
//        fsubp       st(1), st
//        test        edx, edx
//        jns         positive
//        fstp        dword ptr[esp]
//        mov         ecx, dword ptr[esp]
//        xor         ecx, 0x80000000
//        add         ecx, 0x7fffffff
//        adc         eax, 0
//        mov         edx, dword ptr[esp + 0x14]
//        adc         edx, 0
//        jmp         localexit
//    positive:
//        fstp        dword ptr[esp]
//        mov         ecx, dword ptr[esp]
//        add         ecx, 7fffffffh
//        sbb         eax, 0
//        mov         edx, dword ptr[esp + 0x14]
//        sbb         edx, 0
//        jmp         localexit
//    integer_qnan_or_zero:
//        mov         edx, dword ptr[esp + 0x14]
//        test        edx, 0x7fffffff
//        jne         arg_is_not_integer_qnan
//        fstp        dword ptr[esp + 0x18]
//        fstp        dword ptr[esp + 0x18]
//    localexit:
//        leave
//        ret
//    }
//}
//
//RAW void _ftol2_sse()
//{
//    _ftol2();
//}

EXTERN_C_END
