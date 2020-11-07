
#include <stdlib.h>
#include <bios.h>

#include "lib/dos.h"
#include "lib/inttypes.h"
#include "lib/log.h"

// DOS programs can use interrupts in the range 0x60..0x80, although we
// also check to make sure nobody else is using the same interrupt before
// we grab it. Note that the original Doom ipxsetup/sersetup only ever went
// up to 0x66, but we use the full range up to 0x80, which is consistent
// with what DOS packet drivers do.
#define MIN_USER_INTERRUPT  0x60
#define MAX_USER_INTERRUPT  0x80

unsigned char isr_stack_space[ISR_STACK_SIZE];

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

long GetEntropy(void)
{
    long result;
    _bios_timeofday(_TIME_GETCLOCK, &result);
    return result;
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

void far_memcpy(void far *dest, void far *src, size_t nbytes)
{
    uint8_t far *dest_p = (uint8_t far *) dest;
    uint8_t far *src_p = (uint8_t far *) src;
    int i;

    for (i = 0; i < nbytes; ++i)
    {
        *dest_p = *src_p;
        ++dest_p; ++src_p;
    }
}

int far_memcmp(void far *a, void far *b, size_t nbytes)
{
    uint8_t far *a_p = (uint8_t far *) a;
    uint8_t far *b_p = (uint8_t far *) b;
    int i;

    for (i = 0; i < nbytes; ++i)
    {
        if (*a_p != *b_p)
        {
            if (*a_p < *b_p)
            {
                return -1;
            }
            else
            {
                return 1;
            }
        }
        ++a_p; ++b_p;
    }
    return 0;
}

void far_memmove(void far *dest, void far *src, size_t nbytes)
{
    uint8_t far *dest_p = (uint8_t far *) dest;
    uint8_t far *src_p = (uint8_t far *) src;
    int i;

    if (dest < src)
    {
        for (i = 0; i < nbytes; ++i)
        {
            *dest_p = *src_p;
            ++dest_p; ++src_p;
        }
    }
    else
    {
        dest_p += nbytes - 1;
        src_p += nbytes - 1;
        for (i = 0; i < nbytes; ++i)
        {
            *dest_p = *src_p;
            --dest_p; --src_p;
        }
    }
}

void far_bzero(void far *dest, size_t nbytes)
{
    uint8_t far *dest_p = dest;
    int i;

    for (i = 0; i < nbytes; ++i)
    {
        *dest_p = 0;
        ++dest_p;
    }
}

