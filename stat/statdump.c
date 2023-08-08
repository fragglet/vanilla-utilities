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

//
// Implementation of an external statistics driver using Doom's
// -statcopy command line parameter, using the -control API to provide
// an interrupt callback to check the statistics buffer.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/flag.h"
#include "lib/log.h"

#include "ctrl/control.h"
#include "stat/stats.h"
#include "stat/statprnt.h"

// Array of end-of-level statistics that have been captured.
#define MAX_CAPTURES 32
static wbstartstruct_t captured_stats[MAX_CAPTURES];
static int num_captured_stats = 0;

// Output file to write statistics to.  If NULL, print to stdout.
static char *output_filename = NULL;

static void StatsCallback(wbstartstruct_t *stats)
{
    if (num_captured_stats < MAX_CAPTURES)
    {
        memcpy(&captured_stats[num_captured_stats], stats,
               sizeof(wbstartstruct_t));
        ++num_captured_stats;
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
            Error("Failed to open '%s' for write.", output_filename);
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

    SetHelpText("Doom statistics driver",
                "%s -o stats.txt doom2.exe -skill 4");
    StringFlag("-o", &output_filename, "filename",
               "file to write captured statistics");
    ControlRegisterFlags();
    args = ParseCommandLine(argc, argv);
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    // Launch Doom
    StatsLaunchDoom(args, StatsCallback);

    LogMessage("Statistics captured for %i level(s)", num_captured_stats);

    // Write statistics to the output file.
    WriteStats();

    return 0;
}
