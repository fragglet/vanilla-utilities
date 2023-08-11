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

#include <dos.h>

#define ISR_STACK_SIZE 1024

/* SwitchStack() should be called at the start of an ISR function to swap
 * out the stack pointer to point to the isr_stack_space memory block.
 *
 * When an interrupt is serviced, CS and DS are restored so that all our
 * pointers work as expected, but unless we do something, SS:SP still points
 * to the calling program's stack. This means that pointers to data on the
 * stack do not resolve correctly, since DS != SS. While it is possible to
 * carefully avoid taking pointers to anything on the stack, it tends to be
 * fragile and error-prone, with confusing bugs that are hard to track
 * down.  isr_stack_space is located in our data segment so we can set
 * SS = DS while the interrupt is being serviced.
 *
 * After switching to the new stack we push the old SS, SP and BP register
 * values; RestoreStack() will pop them off and restore the old stack.
 */

#if defined(__TURBOC__)

#define RESTORE_ISR_STACK \
    do { \
        asm pop bp; \
        asm pop cx; \
        asm pop bx; \
        asm mov ss, bx; \
        asm mov sp, cx; \
    } while(0)

#define SWITCH_ISR_STACK \
    do { \
        unsigned int nsp = \
            (FP_OFF(isr_stack_space + sizeof(isr_stack_space) - 32)); \
        asm mov ax, nsp; \
        asm mov bx, ss; \
        asm mov cx, sp; \
        asm mov dx, ds; \
        asm mov ss, dx; \
        asm mov sp, ax; \
        asm push bx; \
        asm push cx; \
        asm push bp; \
        asm mov bp, sp; \
    } while(0)

#elif defined(__WATCOMC__)

extern void SwitchStack(unsigned int);
#pragma aux SwitchStack = \
    "mov bx, ss" \
    "mov cx, sp" \
    "mov dx, ds" \
    "mov ss, dx" \
    "mov sp, ax" \
    "push bx" \
    "push cx" \
    "push bp" \
    "mov bp, sp" \
    parm [ax] \
    modify [bx cx dx];

extern void RestoreStack(void);
#pragma aux RestoreStack = \
    "pop bp" \
    "pop cx" \
    "pop bx" \
    "mov ss, bx" \
    "mov sp, cx" \
    modify [bx cx];

extern unsigned int old_stacklow;
extern unsigned int _STACKLOW;  // Watcom-internal

// For Watcom we must override the _STACKLOW variable to point to the bottom
// of the new stack, in order to play nice with Watcom's stack overflow
// detection code that gets included in function headers.
#define SWITCH_ISR_STACK \
    do { \
        SwitchStack(FP_OFF(isr_stack_space + sizeof(isr_stack_space) - 32)); \
        old_stacklow = _STACKLOW; \
        _STACKLOW = FP_OFF(isr_stack_space); \
    } while(0)
#define RESTORE_ISR_STACK \
    do { \
        RestoreStack(); \
        _STACKLOW = old_stacklow; \
    } while(0)

#else

#error No stack switching implemented for this compiler!

#endif

typedef void (interrupt far *interrupt_handler_t)();

struct interrupt_hook
{
    int force_vector;
    int interrupt_num;
    interrupt_handler_t old_isr;
};

int FindAndHookInterrupt(struct interrupt_hook *state,
                         interrupt_handler_t isr);
void RestoreInterrupt(struct interrupt_hook *state);
unsigned int SwitchPSP(void);
void RestorePSP(unsigned int old_psp);

#define PIC_COMMAND_PORT  0x20
#define PIC_DATA_PORT     0x21

struct irq_hook
{
    unsigned int irq;
    interrupt_handler_t old_isr;
    char env_string[14];
    unsigned int was_masked :1;
    unsigned int chained    :1;
};

void HookIRQ(struct irq_hook *state, interrupt_handler_t isr,
             unsigned int irq);
void RestoreIRQ(struct irq_hook *state);
void SetIRQMask(struct irq_hook *irq);
void ClearIRQMask(struct irq_hook *irq);

// In chained mode we call the original ISR and it sends the EOI
// to the PIC. Otherwise we send it ourselves. It is important that
// the interrupt is only acknowledged once, otherwise we can end up
// acknowledging the wrong interrupt.
#define END_OF_IRQ(irq_hook) \
    if ((irq_hook).chained) { \
        _chain_intr((irq_hook).old_isr); \
    } else { \
        OUTPUT(PIC_COMMAND_PORT, 0x60 + (irq_hook).irq); \
    }

extern unsigned char isr_stack_space[ISR_STACK_SIZE];

