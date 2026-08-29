//
// Copyright(C) 2026 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

// This is a "magic" wrapper program that, when run inside DOSbox, redirects
// the DOS stdout to DOSbox's own stdout. This is very useful if you're running
// a headless batch process like a compilation inside DOSbox, because the
// output of that process will be surfaced from DOSbox itself, and not just
// printed to the virtual DOS screen that possibly nobody is able to see. Also,
// unlike redirecting to a file, you'll see the output in real time.

#include <assert.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>

#define DEV_DRIVE "X"

static void atexit_unmount(void)
{
    assert(system("MOUNT -U " DEV_DRIVE) == 0);
}

int main(int argc, char *argv[])
{
    FILE *out;
    int result;
    char cmdbuf[256];

    if (argc < 2)
    {
        printf("Usage: %s <command>\n", argv[0]);
        exit(1);
    }

    // The way we pull off this hack is to mount /dev as the X: drive. We can
    // then use this to open /dev/stdout.
    assert(system("MOUNT " DEV_DRIVE " /dev") == 0);
    atexit(atexit_unmount);

    out = fopen(DEV_DRIVE ":\\stdout", "r+b");
    assert(out != NULL);

    // dup2() lets us replace DOS's own stdout with our handle.
    dup2(fileno(out), fileno(stdout));
    // TODO: Figure out why we can't redirect stderr as well. dosbox quirk?

    _bgetcmd(cmdbuf, sizeof(cmdbuf));
    result = system(cmdbuf);

    fclose(out);
    fclose(stdout);

    exit(result);
}
