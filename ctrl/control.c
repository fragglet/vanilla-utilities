
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
#include <dos.h>
#include <process.h>

#include "ctrl/control.h"

typedef struct {
    long intnum;
    ticcmd_t ticcmd;
} control_buf_t;

static control_buf_t control_buf;

static control_callback_t int_callback;
static void *int_user_data;

static int force_vector = 0;
static void interrupt(*old_isr) ();

// Interrupt service routine.

static void interrupt ControlISR()
{
    memset(&control_buf.ticcmd, 0, sizeof(ticcmd_t));
    int_callback(&control_buf.ticcmd, int_user_data);
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
    int intnum;
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
            fprintf(stderr, "Interrupt vector 0x%x not available!\n",
                    force_vector);
            exit(-1);
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

    fprintf(stderr, "Failed to find an available interrupt vector!\n");
    exit(-1);

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

void ControlRegisterFlags(void)
{
    IntFlag("-cvector", &force_vector, "vector",
            "use the specified interrupt vector");
}

// Launch the game. args[0] is the program to invoke.
void ControlLaunchDoom(char **args, control_callback_t callback,
                       void *user_data)
{
    char addr_string[32];
    long flataddr;
    int intnum;

    intnum = FindInterruptNum();
    printf("Control API: Using interrupt vector 0x%x\n", intnum);

    // Initialize the interrupt handler.

    int_callback = callback;
    int_user_data = user_data;
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
