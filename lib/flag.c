
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include "lib/flag.h"

#define MAX_FLAGS 8

enum flag_type {
    FLAG_BOOL,
    FLAG_STRING,
    FLAG_INT,
};

struct flag {
    enum flag_type type;
    const char *name;
    const char *param_name;
    const char *help_text;
    union {
        char **s;
        int *i;
    } value;
};

static struct flag flags[MAX_FLAGS];
static int num_flags;

static struct flag *NewFlag(const char *name, const char *help_text)
{
    struct flag *f;
    assert(num_flags < MAX_FLAGS);
    f = &flags[num_flags];
    f->name = name;
    f->help_text = help_text;
    ++num_flags;
    return f;
}

void BoolFlag(const char *name, int *ptr, const char *help_text)
{
    struct flag *f = NewFlag(name, help_text);
    f->type = FLAG_BOOL;
    f->value.i = ptr;
}

void IntFlag(const char *name, int *ptr,
             const char *param_name, const char *help_text)
{
    struct flag *f = NewFlag(name, help_text);
    f->type = FLAG_INT;
    f->param_name = param_name;
    f->value.i = ptr;
}

void StringFlag(const char *name, char **ptr,
                const char *param_name, const char *help_text)
{
    struct flag *f = NewFlag(name, help_text);
    f->type = FLAG_STRING;
    f->param_name = param_name;
    f->value.s = ptr;
}

static void Usage(FILE *output, const char *program)
{
    struct flag *f;
    int columns;
    int i, cnt;

    fprintf(output, "Usage: %s [args] [command]\n", program);

    columns = 0;

    for (i = 0; i < num_flags; ++i)
    {
        f = &flags[i];
        cnt = 6 + strlen(f->name);
        if (f->type != FLAG_BOOL)
        {
            cnt += 3 + strlen(f->param_name);
        }
        if (cnt > columns)
        {
            columns = cnt;
        }
    }

    for (i = 0; i < num_flags; ++i)
    {
        f = &flags[i];
        cnt = fprintf(output, "  %s", f->name);
        if (f->type != FLAG_BOOL)
        {
            cnt += fprintf(output, " <%s>", f->param_name);
        }
        while (cnt < columns)
        {
            cnt += fprintf(output, " ");
        }
        fprintf(output, "%s\n", f->help_text);
    }

    fprintf(output, "\nExample:\n");
    fprintf(output, "  %s ", program);
    for (i = 0; i < num_flags; ++i)
    {
        f = &flags[i];
        if (f->type == FLAG_BOOL)
        {
            fprintf(output, "%s ", f->name);
            break;
        }
    }
    fprintf(output, "doom.exe -skill 4 -warp 2 2\n\n");
}

static struct flag *MustFindFlagForName(const char *program, const char *name)
{
    int i;

    for (i = 0; i < num_flags; ++i)
    {
        if (!strcmp(flags[i].name, name))
        {
            return &flags[i];
        }
    }
    fprintf(stderr, "Unknown flag '%s'", name);
    Usage(stderr, program);
    exit(1);
    return NULL;
}

static int MustParseInt(const char *flag_name, char *val)
{
    long result;

    errno = 0;
    // Zero base means 0x hex or 0 oct prefixes are supported:
    result = strtol(val, NULL, 0);
    if (result == 0 && (errno != 0 || result < INT_MIN || result > INT_MAX))
    {
        fprintf(stderr, "Invalid value for flag '%s': '%s'.\n",
                flag_name, val);
        exit(1);
    }

    return (int) result;
}

void ParseCommandLine(int *argc, char ***argv)
{
    struct flag *f;
    long l;
    int i;

    if (*argc == 1)
    {
        goto help;
    }

    for (i = 1; i < *argc; ++i)
    {
        if ((*argv)[i][0] != '-')
        {
            *argc -= i;
            *argv += i;
            return;
        }
        if (!strcmp((*argv)[i], "-h") || !strcmp((*argv)[i], "-help")
         || !strcmp((*argv)[i], "--help"))
        {
            goto help;
        }

        f = MustFindFlagForName((*argv)[0], (*argv)[i]);
        if (f->type != FLAG_BOOL &&
            (i + 1 >= *argc || (*argv)[i + 1][0] == '-'))
        {
            fprintf(stderr, "No value given for '%s'.\n", f->name);
            Usage(stderr, (*argv)[0]);
            exit(1);
        }

        switch (f->type)
        {
            case FLAG_BOOL:
                *f->value.i = 1;
                break;

            case FLAG_INT:
                *f->value.i = MustParseInt(f->name, (*argv)[i + 1]);
                break;

            case FLAG_STRING:
                *f->value.s = (*argv)[i + 1];
                break;
        }
        // Skip over parameter:
        if (f->type != FLAG_BOOL)
        {
            ++i;
        }
    }

    fprintf(stderr, "No command given.\n");
    Usage(stderr, (*argv)[0]);
    exit(1);

help:
    Usage(stdout, (*argv)[0]);
    exit(0);
}

