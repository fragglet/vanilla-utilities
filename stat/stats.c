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
#include <assert.h>

#include "lib/dos.h"
#include "lib/flag.h"
#include "ctrl/control.h"
#include "stat/stats.h"

static stats_callback_t stats_callback;

// Statistics buffer that Doom writes into.
static wbstartstruct_t stats_buffer;

//
// This callback function is invoked in interrupt context by the
// control API interrupt.
//
// We check the "maxfrags" variable, to see if the stats 
// buffer has been written to.  If it has, save the contents of the
// stats buffer into the captured_stats array for later processing.
///
static void far ControlCallback(ticcmd_t *unused)
{
    unused = unused;

    if (stats_buffer.maxfrags == 0)
    {
        // New data has been written to the statistics buffer.
        stats_callback(&stats_buffer);
        stats_buffer.maxfrags = 1;
    }
}

void StatsLaunchDoom(char **args, stats_callback_t callback)
{
    char bufaddr[20];
    long flataddr;

    stats_callback = callback;

    // Launch Doom
    flataddr = (long) FP_SEG(&stats_buffer) * 16 + FP_OFF(&stats_buffer);
    sprintf(bufaddr, "%li", flataddr);
    args = AppendArgs(args, "-statcopy", bufaddr, NULL);

    stats_buffer.maxfrags = 1;

    ControlLaunchDoom(args, ControlCallback);
}

// StatsGetHandle takes the given long value read from the command line
// and returns a wbstartstruct_t pointer.
wbstartstruct_t far *StatsGetHandle(long l)
{
    wbstartstruct_t far *result = NULL;
    unsigned int seg;

    assert(l != 0);
    seg = (int) ((l >> 4) & 0xf000L);
    result = (void far *) MK_FP(seg, l & 0xffffL);

    return result;
}

