
#include <dos.h>

#define INPUT( port )        inp( port )
#define OUTPUT( port, data ) (void) outp( port, data )

typedef void interrupt (*interrupt_handler_t)(void);

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

void far_memcpy(void far *dest, void far *src, size_t nbytes);
int far_memcmp(void far *a, void far *b, size_t nbytes);
void far_memmove(void far *dest, void far *src, size_t nbytes);
void far_bzero(void far *dest, size_t nbytes);

