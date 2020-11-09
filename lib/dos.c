
#include <stdlib.h>
#include <bios.h>

#include "lib/dos.h"
#include "lib/inttypes.h"

long GetEntropy(void)
{
    long result;
    _bios_timeofday(_TIME_GETCLOCK, &result);
    return result;
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

