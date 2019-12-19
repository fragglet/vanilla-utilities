
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

    Program to use the -control API to play back a recorded .lmp
    demo. The game can be continued when the demo finishes.

  */

#include <stdio.h>
#include <stdlib.h>

#include "ctrl/control.h"
#include "lib/flag.h"
#include "lib/log.h"

static uint8_t *demo_buf, *demo_end;
static uint8_t *demo_p;
static char *demo_filename = NULL;
static int is_strife = 0;

static void ReplayCallback(ticcmd_t *ticcmd, void *unused)
{
    int space_needed;

    if (is_strife)
    {
        space_needed = 6;
    }
    else
    {
        space_needed = 4;
    }

    // EOF?

    if (*demo_p == 0x80 || demo_p + space_needed >= demo_end)
    {
        return;
    }

    ticcmd->forwardmove = *demo_p++;
    ticcmd->sidemove = *demo_p++;
    ticcmd->angleturn = (*demo_p++ << 8);
    ticcmd->buttons = *demo_p++;

    if (is_strife)
    {
        ticcmd->buttons2 = *demo_p++;
        ticcmd->inventoryitem = *demo_p++;
    }
}

static void LoadDemo(char *filename)
{
    FILE *fs;
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

    fs = fopen(filename, "rb");

    if (fs == NULL)
    {
        Error("Failed to open %s", filename);
    }

    fseek(fs, 0, SEEK_END);
    len = ftell(fs) - header_size;

    demo_buf = malloc(len);

    if (demo_buf == NULL)
    {
        Error("Failed to allocate demo buffer (%d bytes)", len);
    }

    fseek(fs, header_size, SEEK_SET);
    if (fread(demo_buf, len, 1, fs) < 1)
    {
        Error("Failed to read entire demo from %s", filename);
    }

    demo_p = demo_buf;
    demo_end = demo_buf + len;
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

    LoadDemo(demo_filename);

    ControlLaunchDoom(args, ReplayCallback, NULL);

    return 0;
}
