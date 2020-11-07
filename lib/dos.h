
#include <conio.h>
#include <dos.h>

#define ISR_STACK_SIZE 2048

#if defined(__TURBOC__)
#define cmdline_argc  _argc
#define cmdline_argv  _argv
#define __stdcall
#else
#define cmdline_argc  __argc
#define cmdline_argv  __argv
#endif

#define strcasecmp stricmp
#define strncasecmp strnicmp

#define INPUT( port )        inp( port )
#define OUTPUT( port, data ) (void) outp( port, data )

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

#define SwitchStack(new_sp) \
    do { \
        unsigned int nsp = (new_sp); \
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

#define RestoreStack() \
    do { \
        asm pop bp; \
        asm pop cx; \
        asm pop bx; \
        asm mov sp, cx; \
        asm mov ss, bx; \
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
    "mov sp, cx" \
    "mov ss, bx" \
    modify [bx cx];

#else

#error No stack switching implemented for this compiler!

#endif

typedef void (interrupt far *interrupt_handler_t)(void);

struct interrupt_hook
{
    int interrupt_num;
    interrupt_handler_t old_isr;
    int force_vector;
};

int FindAndHookInterrupt(struct interrupt_hook *state,
                         interrupt_handler_t isr);
void RestoreInterrupt(struct interrupt_hook *state);
long GetEntropy(void);

extern unsigned char isr_stack_space[ISR_STACK_SIZE];
#define SWITCH_ISR_STACK \
	SwitchStack(FP_OFF(isr_stack_space + sizeof(isr_stack_space) - 32))
#define RESTORE_ISR_STACK RestoreStack()

void far_memcpy(void far *dest, void far *src, size_t nbytes);
int far_memcmp(void far *a, void far *b, size_t nbytes);
void far_memmove(void far *dest, void far *src, size_t nbytes);
void far_bzero(void far *dest, size_t nbytes);

