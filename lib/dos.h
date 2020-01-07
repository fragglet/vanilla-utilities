
#include <conio.h>
#include <dos.h>

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

void far_memcpy(void far *dest, void far *src, size_t nbytes);
int far_memcmp(void far *a, void far *b, size_t nbytes);
void far_memmove(void far *dest, void far *src, size_t nbytes);
void far_bzero(void far *dest, size_t nbytes);

