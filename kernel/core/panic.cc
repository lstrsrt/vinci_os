#include "ke.h"
#include "../hw/gfx/output.h"
#include "../hw/cpu/x64.h"

namespace ke
{
    NO_RETURN void Panic(Status status)
    {
        Print("Kernel panic! Status: %d\n", status);
        x64::Halt();
    }

    NO_RETURN void Panic(Status status, size_t p1, size_t p2, size_t p3, size_t p4)
    {
        Print(
            "Kernel panic! Status: %d\n"
            "P1: 0x%llx\n"
            "P2: 0x%llx\n"
            "P3: 0x%llx\n"
            "P4: 0x%llx\n",
            status,
            p1, p2, p3, p4
        );
        x64::Halt();
    }
}
