
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

    p = strrchr(_argv[0], '\\');
    if (p != NULL)
    {
        ++p;
    }
    else
    {
        p = _argv[0];
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

void SetLogDistinguisher(char *name)
{
    strncpy(distinguisher, name, sizeof(distinguisher));
    distinguisher[sizeof(distinguisher) - 1] = '\0';
}

static void LogVarargs(char *fmt, va_list args)
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

void LogMessage(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogVarargs(fmt, args);
    va_end(args);
}

// Aborts the program with an abnormal program termination.
void Error(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogVarargs(fmt, args);
    va_end(args);

    exit(1);
}

// Aborts the program with an error due to wrong command line arguments.
void ErrorPrintUsage(char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    LogVarargs(fmt, args);
    va_end(args);

    PrintProgramUsage(stderr);

    exit(1);
}

void CheckAbort(char *operation)
{
    while (_bios_keybrd(_KEYBRD_READY))
    {
        if ((_bios_keybrd(_KEYBRD_READ) & 0xff) == 27)
        {
            Error("%s aborted.", operation);
        }
    }
}

