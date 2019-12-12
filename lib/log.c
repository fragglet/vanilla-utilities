
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <dos.h>

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

void LogMessage(char *fmt, ...)
{
    va_list args;

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

    va_start(args, fmt);
    vfprintf(log, fmt, args);
    va_end(args);

    fprintf(log, "\n");
}

