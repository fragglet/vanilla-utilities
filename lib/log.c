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
#include <ctype.h>
#include <stdarg.h>
#include <bios.h>

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"

static FILE *log = stdout;
static char progname[9] = "", distinguisher[10] = "";

static void SetLogName(void)
{
    char *p;

    p = strrchr(cmdline_argv[0], '\\');
    if (p != NULL)
    {
        ++p;
    }
    else
    {
        p = cmdline_argv[0];
    }
    strncpy(progname, p, sizeof(progname));
    progname[sizeof(progname) - 1] = '\0';

    // Cut off file extension
    p = strchr(progname, '.');
    if (p != NULL)
    {
        *p = '\0';
    }

    for (p = progname; *p != '\0'; ++p)
    {
        *p = tolower(*p);
    }
}

void SetLogDistinguisher(const char *name)
{
    strncpy(distinguisher, name, sizeof(distinguisher));
    distinguisher[sizeof(distinguisher) - 1] = '\0';
}

static void LogVarargs(const char *fmt, va_list args)
{
    if (strlen(progname) == 0)
    {
        SetLogName();
    }

    fprintf(log, "%s", progname);
    if (strlen(distinguisher) > 0)
    {
        fprintf(log, "[%s]", distinguisher);
    }
    fprintf(log, ": ");

    vfprintf(log, fmt, args);

    fprintf(log, "\n");
}

void LogMessage(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogVarargs(fmt, args);
    va_end(args);
}

// Aborts the program with an abnormal program termination.
void Error(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogVarargs(fmt, args);
    va_end(args);

    exit(1);
}

// Aborts the program with an error due to wrong command line arguments.
void ErrorPrintUsage(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogVarargs(fmt, args);
    va_end(args);

    PrintProgramUsage(stderr);

    exit(1);
}

static void DosIdle(void)
{
    static int dos_major_version = 0;
    union REGS inregs, outregs;

    // Invoke DOS idle interrupt:
    int86(0x28, &inregs, &outregs);

    if (dos_major_version == 0)
    {
        inregs.h.ah = 0x30;   // GET DOS VERSION
        inregs.h.al = 0;
        outregs.h.al = 0;
        int86(0x21, &inregs, &outregs);
        dos_major_version = outregs.h.al;
    }

    // Release current virtual machine timeslice.
    // Note that this is also supported under older DOS & Windows versions,
    // but we only call it on Win9x ("DOS 7") because,
    // " When called very often without intermediate screen output under MS
    //   Windows 3.x, the VM will go into an idle-state and will not receive
    //   the next slice before 8 seconds have elapsed. "
    if (dos_major_version >= 7)
    {
        inregs.x.ax = 0x1680;
        int86(0x2f, &inregs, &outregs);
    }
}

void CheckAbort(const char *operation)
{
    while (_bios_keybrd(_KEYBRD_READY))
    {
        if ((_bios_keybrd(_KEYBRD_READ) & 0xff) == 27)
        {
            Error("%s aborted.", operation);
        }
    }

    DosIdle();
}

