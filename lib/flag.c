
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <dos.h>

#include "lib/flag.h"
#include "lib/log.h"

#define MAX_FLAGS 20

enum flag_type {
    FLAG_BOOL,
    FLAG_STRING,
    FLAG_INT,
    FLAG_API_POINTER,
};

struct flag {
    enum flag_type type;
    const char *name;
    const char *param_name;
    const char *help_text;
    union {
        char **s;
        int *i;
        api_pointer_callback_t callback;
    } value;
};

static char *description, *example;
static struct flag flags[MAX_FLAGS];
static int num_flags;

void SetHelpText(char *program_description, char *example_cmd)
{
    description = program_description;
    example = example_cmd;
}

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

void APIPointerFlag(const char *name, api_pointer_callback_t callback)
{
    struct flag *f = NewFlag(name, NULL);
    f->type = FLAG_API_POINTER;
    f->param_name = "flataddr";
    f->value.callback = callback;
}

void PrintProgramUsage(FILE *output)
{
    const char *program = _argv[0];
    struct flag *f;
    int columns;
    int i, cnt;

    if (description != NULL)
    {
        fprintf(output, "%s\n\n", description);
    }
    fprintf(output, "Usage: %s [args] [command]\n", program);

    columns = 0;

    for (i = 0; i < num_flags; ++i)
    {
        f = &flags[i];
        cnt = 4 + strlen(f->name);
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
        if (f->help_text == NULL)
        {
            continue;
        }
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

    if (example != NULL)
    {
        fprintf(output, "\nExample:\n  ");
        fprintf(output, example, program);
        fprintf(output, "\n\n");
    }
}

static struct flag *FindFlagForName(const char *name)
{
    int i;

    for (i = 0; i < num_flags; ++i)
    {
        if (!strcmp(flags[i].name, name))
        {
            return &flags[i];
        }
    }
    return NULL;
}

static struct flag *MustFindFlagForName(const char *name)
{
    struct flag *result = FindFlagForName(name);
    if (result == NULL)
    {
        ErrorPrintUsage("Unknown flag '%s'", name);
    }
    return result;
}

static int MustParseInt(const char *flag_name, char *val)
{
    long result;

    errno = 0;
    // Zero base means 0x hex or 0 oct prefixes are supported:
    result = strtol(val, NULL, 0);
    if (result == 0 && (errno != 0 || result < INT_MIN || result > INT_MAX))
    {
        ErrorPrintUsage("Invalid value for flag '%s': '%s'.", flag_name, val);
    }

    return (int) result;
}

static void StripAPIPointers(int *argc, char **argv)
{
    struct flag *f;
    long l;
    int i, j;

    // API pointer flags are handled separately, because they are expected
    // to appear at a different position on the command line since they
    // have not been typed by a human. For example:
    //   foo.exe -bar doom2.exe -warp 1 -control 123456
    // In this example, ParseCommandLine stops at 'doom2.exe', but the
    // -control argument appears after it. So we handle these specially and
    // strip them out before ParseCommandLine() even does its thing.

    for (i = 1, j = 1; i < *argc; ++i)
    {
        if (argv[i][0] == '-')
        {
            f = FindFlagForName(argv[i]);
            if (f != NULL && f->type == FLAG_API_POINTER)
            {
                l = strtol(argv[i + 1], NULL, 10);
                assert(l != 0);
                f->value.callback(l);
                ++i;
                continue;
            }
        }

        argv[j] = argv[i];
        ++j;
    }

    *argc = j;
}

char **ParseCommandLine(int argc, char **argv)
{
    struct flag *f;
    int i;

    if (argc == 1)
    {
        goto help;
    }

    StripAPIPointers(&argc, argv);

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
        {
            return AppendArgList(NULL, argc - i, argv + i);
        }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help")
         || !strcmp(argv[i], "--help"))
        {
            goto help;
        }

        f = MustFindFlagForName(argv[i]);
        if (f->type != FLAG_BOOL &&
            (i + 1 >= argc || argv[i + 1][0] == '-'))
        {
            ErrorPrintUsage("No value given for '%s'.", f->name);
        }

        switch (f->type)
        {
            case FLAG_BOOL:
                *f->value.i = 1;
                break;

            case FLAG_INT:
                *f->value.i = MustParseInt(f->name, argv[i + 1]);
                break;

            case FLAG_STRING:
                *f->value.s = argv[i + 1];
                break;

            case FLAG_API_POINTER:
                // Handled in StripAPIPointers().
                break;
        }
        // Skip over parameter:
        if (f->type != FLAG_BOOL)
        {
            ++i;
        }
    }

    // No follow-on command was given. This may be an error but it's up to
    // the caller to decide.
    return NULL;

help:
    PrintProgramUsage(stdout);
    exit(0);
    return NULL;  // unreachable
}

static char **ExtendArgList(char **args, int *current_argc, int extra_args)
{
    int i;

    *current_argc = 0;

    if (args != NULL)
    {
        for (i = 0; args[i] != NULL; ++i)
        {
            ++*current_argc;
        }
    }

    args = realloc(args, sizeof(*args) * (*current_argc + extra_args + 1));
    assert(args != NULL);
    return args;
}

char **AppendArgList(char **args, int argc, char **argv)
{
    int current_argc;

    args = ExtendArgList(args, &current_argc, argc);
    memcpy(&args[current_argc], argv, argc * sizeof(*argv));
    args[current_argc + argc] = NULL;

    return args;
}

char **AppendArgs(char **args, ...)
{
    va_list a;
    int current_argc, argc;
    int i;
    char *arg;

    va_start(a, args);
    argc = 0;
    while (va_arg(a, char *) != NULL)
    {
        ++argc;
    }
    va_end(a);

    args = ExtendArgList(args, &current_argc, argc);
    va_start(a, args);
    for (i = current_argc;; ++i)
    {
        arg = va_arg(a, char *);
        if (arg == NULL)
        {
            break;
        }
        args[i] = arg;
    }
    args[i] = NULL;
    va_end(a);

    return args;
}

