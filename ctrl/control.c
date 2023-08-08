//
// Copyright(C) 2011-2023 Simon Howard
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

// Functions for interacting with the Doom -control API.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <assert.h>

#include "ctrl/control.h"
#include "lib/dos.h"
#include "lib/ints.h"
#include "lib/flag.h"
#include "lib/log.h"

struct control_handle_s
{
    long intnum;
    ticcmd_t ticcmd;
};

static struct interrupt_hook control_interrupt;
static control_handle_t control_buf;
static control_handle_t far *next_handle;

static control_callback_t int_callback;

// Interrupt service routine.

static void interrupt far ControlISR()
{
    SWITCH_ISR_STACK;

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

    // The callback may invoke DOS API functions to read/write files,
    // so switch back to our PSP for the duration of the callback.
    {
        unsigned int saved_psp = SwitchPSP();
        int_callback(&control_buf.ticcmd);
        RestorePSP(saved_psp);
    }

    RESTORE_ISR_STACK;
}

static void ControlPointerCallback(long l)
{
    assert(next_handle == NULL);
    next_handle = ControlGetHandle(l);
}

void ControlRegisterFlags(void)
{
    APIPointerFlag("-control", ControlPointerCallback);
    IntFlag("-cvector", &control_interrupt.force_vector, "vector", NULL);
}

// Launch the game. args[0] is the program to invoke.
void ControlLaunchDoom(char **args, control_callback_t callback)
{
    char addr_string[32];
    long flataddr;

    if (!FindAndHookInterrupt(&control_interrupt, ControlISR))
    {
        Error("Failed to find a free DOS interrupt. Try using -cvector "
              "to manually force an interrupt.");
    }

    // Initialize the interrupt handler.

    int_callback = callback;
    control_buf.intnum = control_interrupt.interrupt_num;

    // Add the -control argument.

    flataddr = (long) FP_SEG(&control_buf) * 16 + FP_OFF(&control_buf);
    sprintf(addr_string, "%li", flataddr);
    args = AppendArgs(args, "-control", addr_string, NULL);

    SquashToResponseFile(args);

    // Launch Doom:
    spawnv(P_WAIT, args[0], args);

    free(args);

    RestoreInterrupt(&control_interrupt);
}

// ControlGetHandle takes the given long value read from the command line
// and returns a control_handle_t pointer.
control_handle_t far *ControlGetHandle(long l)
{
    control_handle_t far *result = NULL;
    unsigned int seg;

    assert(l != 0);
    seg = (int) ((l >> 4) & 0xf000L);
    result = (void far *) MK_FP(seg, l & 0xffffL);

    return result;
}

void ControlInvoke(control_handle_t far *handle, ticcmd_t *ticcmd)
{
    union REGS regs;
    far_memcpy(&handle->ticcmd, ticcmd, sizeof(ticcmd_t));
    int86((int) handle->intnum, &regs, &regs);
    far_memcpy(ticcmd, &handle->ticcmd, sizeof(ticcmd_t));
}

