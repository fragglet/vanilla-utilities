#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static void ControlCallback(ticcmd_t *unused)
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

