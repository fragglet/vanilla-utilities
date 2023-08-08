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
//

// Program to use the -control API to play back a recorded .lmp
// demo. The game can be continued when the demo finishes.

#include <stdio.h>
#include <stdlib.h>

#include "ctrl/control.h"
#include "lib/flag.h"
#include "lib/log.h"

static FILE *demo_stream;
static char *demo_filename = NULL;
static int is_strife = 0;

static void far ReplayCallback(ticcmd_t *ticcmd)
{
    unsigned char ticbuf[6];
    int ticbuf_size, nbytes;

    if (is_strife)
    {
        ticbuf_size = 6;
    }
    else
    {
        ticbuf_size = 4;
    }

    nbytes = fread(ticbuf, 1, ticbuf_size, demo_stream);

    // EOF?
    if (nbytes < ticbuf_size || ticbuf[0] == 0x80)
    {
        return;
    }

    ticcmd->forwardmove = ticbuf[0];
    ticcmd->sidemove = ticbuf[1];
    ticcmd->angleturn = (ticbuf[2] << 8);
    ticcmd->buttons = ticbuf[3];

    if (is_strife)
    {
        ticcmd->buttons2 = ticbuf[4];
        ticcmd->inventoryitem = ticbuf[5];
    }
}

static void OpenDemo(char *filename)
{
    size_t len;
    int header_size;

    if (is_strife)
    {
        header_size = 16;
    }
    else
    {
        header_size = 13;
    }

    demo_stream = fopen(filename, "rb");

    if (demo_stream == NULL)
    {
        Error("Failed to open %s", filename);
    }

    fseek(demo_stream, header_size, SEEK_SET);
}

int main(int argc, char *argv[])
{
    char **args;

    SetHelpText("Replay demo through Doom control API",
                "%s -playdemo old.lmp doom.exe -record new.lmp");
    StringFlag("-playdemo", &demo_filename,
               "filename", "play back the specified demo file");
    BoolFlag("-strife", &is_strife, "play back a Strife demo");
    ControlRegisterFlags();
    args = ParseCommandLine(argc, argv);

    if (demo_filename == NULL)
    {
        ErrorPrintUsage("Demo file not specified.");
    }
    if (args == NULL)
    {
        ErrorPrintUsage("No command given to run.");
    }

    OpenDemo(demo_filename);
    ControlLaunchDoom(args, ReplayCallback);
    fclose(demo_stream);

    return 0;
}
