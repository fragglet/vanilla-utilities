
#include <stdlib.h>
#include <bios.h>

#include "lib/inttypes.h"
#include "lib/ints.h"
#include "lib/log.h"

// DOS programs can use interrupts in the range 0x60..0x80, although we
// also check to make sure nobody else is using the same interrupt before
// we grab it. Note that the original Doom ipxsetup/sersetup only ever went
// up to 0x66, but we use the full range up to 0x80, which is consistent
// with what DOS packet drivers do.
#define MIN_USER_INTERRUPT  0x60
#define MAX_USER_INTERRUPT  0x80

unsigned char isr_stack_space[ISR_STACK_SIZE];

#ifdef __WATCOMC__
unsigned int old_stacklow;
#endif

static int FindFreeInterrupt(void)
{
    int i;

    for (i = MIN_USER_INTERRUPT; i <= MAX_USER_INTERRUPT; ++i)
    {
        if (_dos_getvect(i) == NULL)
        {
            return i;
        }
    }
    LogMessage("No free interrupt handler found in the 0x%x..0x%x range.",
               MIN_USER_INTERRUPT, MAX_USER_INTERRUPT);
    return 0;
}

int FindAndHookInterrupt(struct interrupt_hook *state,
                         interrupt_handler_t isr)
{
    if (state->force_vector != 0)
    {
        LogMessage("Using forced interrupt vector 0x%x", state->force_vector);
        state->interrupt_num = state->force_vector;
    }
    else
    {
        state->interrupt_num = FindFreeInterrupt();
        if (state->interrupt_num == 0)
        {
            return 0;
        }
    }

    state->old_isr = _dos_getvect(state->interrupt_num);
    _dos_setvect(state->interrupt_num, isr);
    return 1;
}

void RestoreInterrupt(struct interrupt_hook *state)
{
    if (state->interrupt_num == 0)
    {
        return;
    }
    _dos_setvect(state->interrupt_num, state->old_isr);
    state->interrupt_num = 0;
}

#define DOS_INTERRUPT_API  0x21
#define DOS_API_SET_CURRENT_PROCESS  0x50
#define DOS_API_GET_CURRENT_PROCESS  0x51

void RestorePSP(unsigned int psp)
{
    union REGS regs;
    regs.h.ah = DOS_API_SET_CURRENT_PROCESS;
    regs.x.bx = psp;
    int86(DOS_INTERRUPT_API, &regs, &regs);
}

unsigned int SwitchPSP(void)
{
    union REGS regs;

    regs.h.ah = DOS_API_GET_CURRENT_PROCESS;
    int86(DOS_INTERRUPT_API, &regs, &regs);

    // Now we switch back to our own PSP during the lifetime of the ISR.
    RestorePSP(_psp);

    return regs.x.bx;
}

