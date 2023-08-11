//
// Copyright(C) 2019-2023 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bios.h>
#include <assert.h>

#include "lib/dos.h"
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

static int CheckChainedIRQ(unsigned int irq)
{
    char name[14];
    char *value;

    sprintf(name, "CHAIN_IRQ%d", irq);
    value = getenv(name);
    return value != NULL && !strcmp(value, "1");
}

static void SetChainedIRQ(struct irq_hook *state, int enable)
{
    sprintf(state->env_string, "CHAIN_IRQ%d=%s", state->irq,
            enable ? "1" : "");
    putenv(state->env_string);
}

void HookIRQ(struct irq_hook *state, interrupt_handler_t isr,
             unsigned int irq)
{
    assert(irq < 8);

    // Usually, for efficiency, we want the hardware all to ourselves and
    // deliberately don't chain to call whatever interrupt handler was
    // there before us. However, we want to play nicely with other copies
    // of the same binary that are sharing the same interrupt (eg. two
    // SERSETUPs on COM1 and COM3). So the first time that we hook the
    // interrupt we set a special environment variable. Other instances
    // then detect this and set chaining mode.
    // Doing this also ensures that we send the EOI message to the PIC only
    // once (see END_OF_IRQ() macro).
    state->irq = irq;
    state->chained = CheckChainedIRQ(irq);
    if (!state->chained)
    {
        SetChainedIRQ(state, 1);
    }

    _disable();

    state->was_masked = (INPUT(PIC_DATA_PORT) & (1 << irq)) != 0;
    state->old_isr = _dos_getvect(8 + irq);
    _dos_setvect(8 + irq, isr);

    ClearIRQMask(state);

    _enable();
}

void RestoreIRQ(struct irq_hook *state)
{
    _disable();

    SetIRQMask(state);
    _dos_setvect(8 + state->irq, state->old_isr);

    if (!state->was_masked)
    {
        ClearIRQMask(state);
    }

    _enable();

    // We can't delete an environment variable but we can set it to zero.
    // We only do this if we previously set the variable ourselves.
    if (!state->chained)
    {
        SetChainedIRQ(state, 0);
    }
}

void SetIRQMask(struct irq_hook *irq)
{
    OUTPUT(PIC_DATA_PORT, INPUT(PIC_DATA_PORT) | (1 << irq->irq));
}

void ClearIRQMask(struct irq_hook *irq)
{
    OUTPUT(PIC_DATA_PORT, INPUT(PIC_DATA_PORT) & ~(1 << irq->irq));
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

