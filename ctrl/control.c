
 /* 

    Copyright(C) 2011 Simon Howard

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
    02111-1307, USA.

    --

    Functions for interacting with the Doom -control API.

  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <process.h>
#include <assert.h>

#include "ctrl/control.h"
#include "lib/flag.h"
#include "lib/log.h"

struct control_handle_s
{
    long intnum;
    ticcmd_t ticcmd;
};

static control_handle_t control_buf;
static control_handle_t far *next_handle;

static control_callback_t int_callback;

static int force_vector = 0;
static void interrupt(*old_isr) ();

// Interrupt service routine.

static void interrupt ControlISR()
{
    // If we have a next_handle, invoke the next -control driver in the
    // chain to populate the ticcmd before we invoke our callback
    // function. Otherwise, we just start by zeroing out the ticcmd.
    if (next_handle != NULL)
    {
        ControlInvoke(next_handle, &control_buf.ticcmd);
    }
    else
    {
        memset(&control_buf.ticcmd, 0, sizeof(ticcmd_t));
    }
    int_callback(&control_buf.ticcmd);
}

// Check if an interrupt vector is available.

static int InterruptVectorAvailable(int intnum)
{
    unsigned char far *vector;

    vector = *((char far * far *)(intnum * 4));

    return vector == NULL || *vector == 0xcf;
}

// Find what interrupt number to use.

static int FindInterruptNum(void)
{
    int i;

    // -cvector specified in parameters?
    if (force_vector)
    {
        if (InterruptVectorAvailable(force_vector))
        {
            return force_vector;
        }
        else
        {
            Error("Interrupt vector 0x%x not available!", force_vector);
        }
    }

    // Figure out a vector to use automatically.
    for (i = 0x60; i <= 0x66; ++i)
    {
        if (InterruptVectorAvailable(i))
        {
            return i;
        }
    }

    Error("Failed to find an available interrupt vector!");
    return -1;
}

// Install the interrupt handler on the specified interrupt vector.

static void HookInterruptHandler(int intnum)
{
    void interrupt(*isr) ();

    old_isr = getvect(intnum);

    isr = MK_FP(_CS, (int)ControlISR);
    setvect(intnum, isr);
}

// Unload the interrupt handler.

static void RestoreInterruptHandler(int intnum)
{
    setvect(intnum, old_isr);
}

static void ControlPointerCallback(long l)
{
    assert(next_handle == NULL);
    next_handle = ControlGetHandle(l);
}

void ControlRegisterFlags(void)
{
    APIPointerFlag("-control", ControlPointerCallback);
    IntFlag("-cvector", &force_vector, "vector",
            "use the specified interrupt vector");
}

// Launch the game. args[0] is the program to invoke.
void ControlLaunchDoom(char **args, control_callback_t callback)
{
    char addr_string[32];
    long flataddr;
    int intnum;

    intnum = FindInterruptNum();
    LogMessage("Using interrupt vector 0x%x for control API", intnum);

    // Initialize the interrupt handler.

    int_callback = callback;
    control_buf.intnum = intnum;
    HookInterruptHandler(intnum);

    // Add the -control argument.

    flataddr = (long)_DS * 16 + (unsigned)(&control_buf);
    sprintf(addr_string, "%li", flataddr);
    args = AppendArgs(args, "-control", addr_string, NULL);

    // Launch Doom:
    spawnv(P_WAIT, args[0], args);

    free(args);

    RestoreInterruptHandler(intnum);
}

// ControlGetHandle takes the given long value read from the command line
// and returns a control_handle_t pointer.
control_handle_t far *ControlGetHandle(long l)
{
    control_handle_t far *result = NULL;
    unsigned int seg;

    assert(l != 0);
    seg = (int) ((l >> 4) & 0xf000L);
    result = MK_FP(seg, l & 0xffffL);

    return result;
}

static void far_memcpy(void far *dest, void far *src, size_t nbytes)
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

void ControlInvoke(control_handle_t far *handle, ticcmd_t *ticcmd)
{
    union REGS regs;
    far_memcpy(&handle->ticcmd, ticcmd, sizeof(ticcmd_t));
    int86((int) handle->intnum, &regs, &regs);
    far_memcpy(ticcmd, &handle->ticcmd, sizeof(ticcmd_t));
}

