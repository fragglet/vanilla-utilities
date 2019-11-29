
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

static byte *demo_buf, *demo_end;
static byte *demo_p;
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
        fprintf(stderr, "Failed to open %s\n", filename);
        exit(-1);
    }

    fseek(fs, 0, SEEK_END);
    len = ftell(fs) - header_size;

    demo_buf = malloc(len);

    if (demo_buf == NULL)
    {
        fprintf(stderr, "Failed to allocate demo buffer\n");
        exit(-1);
    }

    fseek(fs, header_size, SEEK_SET);
    if (fread(demo_buf, len, 1, fs) < 1)
    {
        fprintf(stderr, "Failed to read entire demo\n");
        exit(-1);
    }

    demo_p = demo_buf;
    demo_end = demo_buf + len;
}

int main(int argc, char *argv[])
{
    char **args;

    StringFlag("-playdemo", &demo_filename,
               "filename", "play back the specified demo file");
    BoolFlag("-strife", &is_strife, "play back a Strife demo");
    ControlRegisterFlags();
    args = ParseCommandLine(argc, argv);

    if (demo_filename == NULL)
    {
        printf("Demo file not specified.\n");
        exit(1);
    }

    LoadDemo(demo_filename);

    ControlLaunchDoom(args, ReplayCallback, NULL);

    return 0;
}
