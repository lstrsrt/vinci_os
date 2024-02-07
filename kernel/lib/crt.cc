#include "base.h"

/* CRT symbols for compatibility with LINK.exe. */

EXTERN_C_START

void __chkstk()
{

}

__declspec(selectany) int _fltused = 1;

EXTERN_C_END
