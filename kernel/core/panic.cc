#include "ke.h"
#include "gfx/output.h"
#include "../hw/cpu/x64.h"

namespace ke
{
    void Panic(Status status)
    {
        Print("Kernel panic! Status: %d\n", status);
        x64::Halt();
    }
}
