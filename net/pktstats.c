//
// Copyright(C) 2025 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#include <stdio.h>
#include <stdlib.h>

#include "lib/flag.h"
#include "lib/log.h"
#include "net/pktstats.h"

static struct counter *counters = NULL;
static int print_stats_on_exit = 0;

static void PrintStats(void)
{
    int printed_header = 0;
    struct counter *c;

    if (!print_stats_on_exit)
    {
        return;
    }

    for (c = counters; c != NULL; c = c->next)
    {
        if (c->i == 0)
        {
            continue;
        }
        if (!printed_header)
        {
            LogMessage("Statistics:");
            printed_header = 1;
        }
        LogMessage("%16s %6ld", c->name, c->i);
    }
}

void RegisterCounter(struct counter *ctr)
{
    ctr->next = counters;
    counters = ctr;
}

void PacketStatsRegisterFlags(void)
{
    BoolFlag("-stats", &print_stats_on_exit, "Print statistics on exit");
    atexit(PrintStats);
}
