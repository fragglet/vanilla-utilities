
 /* 

    Copyright(C) 2007,2011 Simon Howard

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

    Implementation of an external statistics driver using Doom's
    -statcopy command line parameter, using the -control API to provide
    an interrupt callback to check the statistics buffer.

  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#include "ctrl/control.h"
#include "lib/flag.h"
#include "stat/stats.h"
#include "stat/statprnt.h"

// Array of end-of-level statistics that have been captured.
#define MAX_CAPTURES 32
static wbstartstruct_t captured_stats[MAX_CAPTURES];
static int num_captured_stats = 0;

// Statistics buffer that Doom writes into.
static wbstartstruct_t stats_buffer;

// Output file to write statistics to.  If NULL, print to stdout.
static char *output_filename = NULL;

//
// This callback function is invoked in interrupt context by the
// control API interrupt.
//
// We check the "maxfrags" variable, to see if the stats 
// buffer has been written to.  If it has, save the contents of the
// stats buffer into the captured_stats array for later processing.
///
static void ControlCallback(ticcmd_t *ticcmd, void *unused)
{
    if (stats_buffer.maxfrags == 0)
    {
        // New data has been written to the statistics buffer.
        // Save it for later processing.

        if (num_captured_stats < MAX_CAPTURES)
        {
            memcpy(&captured_stats[num_captured_stats], &stats_buffer,
                   sizeof(wbstartstruct_t));
            ++num_captured_stats;
        }

        stats_buffer.maxfrags = 1;
    }
}

// Write the statistics to the output file.
static void WriteStats(void)
{
    FILE *outfile;
    int i;

    // Open the output file for writing.  If none is specified,
    // write the data to stdout.

    if (output_filename != NULL)
    {
        outfile = fopen(output_filename, "w");

        if (outfile == NULL)
        {
            fprintf(stderr, "Failed to open '%s' for write.\n",
                    output_filename);
            return;
        }
    }
    else
    {
        outfile = stdout;
    }

    // Work out if this was Doom 1 or Doom 2.

    DiscoverGamemode(captured_stats, num_captured_stats);

    // Write the statistics for each level to the file.

    for (i = 0; i < num_captured_stats; ++i)
    {
        PrintStats(outfile, &captured_stats[i]);
    }

    // Close the output file

    if (output_filename != NULL)
    {
        fclose(outfile);
    }
}

int main(int argc, char *argv[])
{
    char **args;
    char bufaddr[20];
    long flataddr;

    StringFlag("-o", &output_filename, "filename",
               "file to write captured statistics");
    ControlRegisterFlags();
    args = ParseCommandLine(argc, argv);

    // Launch Doom
    flataddr = (long)_DS *16 + (unsigned)(&stats_buffer);
    sprintf(bufaddr, "%li", flataddr);
    args = AppendArgs(args, "-statcopy", bufaddr, NULL);

    stats_buffer.maxfrags = 1;

    ControlLaunchDoom(args, ControlCallback, NULL);

    printf("Statistics captured for %i level(s)\n", num_captured_stats);

    // Write statistics to the output file.
    WriteStats();

    return 0;
}
