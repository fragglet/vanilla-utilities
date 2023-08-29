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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include "lib/bakedin.h"
#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"

#define MAX_COMMAND_LINE_LEN 126
#define MAX_FLAGS 24

// "Baked in" config contains command line arguments that are always added
// to the command line. The .exe can be modified to change them.
static const struct baked_in_config baked_in_config = {
    BAKED_IN_MAGIC1 BAKED_IN_MAGIC2,
    {0, 0},
};

enum flag_type {
    FLAG_BOOL,
    FLAG_STRING,
    FLAG_UNSIGNED_INT,
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
static char *response_file_arg = NULL;

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

void UnsignedIntFlag(const char *name, unsigned int *ptr,
                     const char *param_name, const char *help_text)
{
    struct flag *f = NewFlag(name, help_text);
    f->type = FLAG_UNSIGNED_INT;
    f->param_name = param_name;
    f->value.i = (int *) ptr;
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
    const char *program = cmdline_argv[0];
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
        if (f->type != FLAG_BOOL && f->param_name != NULL)
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
        if (f->type != FLAG_BOOL && f->param_name != NULL)
        {
            cnt += fprintf(output, " <%s>", f->param_name);
        }
        while (cnt < columns)
        {
            cnt += fprintf(output, " ");
        }
        fprintf(output, "%s\n", f->help_text);
    }

    // We don't display the example when using a baked-in config, as the
    // extra arguments can change the command syntax.
    if (!HAVE_BAKED_IN_CONFIG(baked_in_config) && example != NULL)
    {
        fprintf(output, "\nExample:\n  ");
        fprintf(output, example, program);
        fprintf(output, "\n");
    }
}

static struct flag *FindFlagForName(const char *name)
{
    int i;

    for (i = 0; i < num_flags; ++i)
    {
        if (!strcasecmp(flags[i].name, name))
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

static int MustParseInt(const char *flag_name, char *val, long min, long max)
{
    char *endptr;
    long result;

    errno = 0;
    // Zero base means 0x hex or 0 oct prefixes are supported:
    result = strtol(val, &endptr, 0);
    if ((result == 0 && errno != 0) || endptr == val || *endptr != '\0')
    {
        ErrorPrintUsage("Invalid value for flag '%s': '%s'.", flag_name, val);
    }
    if (result < min || result > max)
    {
        ErrorPrintUsage("Value out of range for flag '%s': %li not "
                        "in range %li - %li.", flag_name, result, min, max);
    }
    return (int) result;
}

static char **ReadResponseFile(char *filename)
{
    char **result = NULL;
    FILE *fs;
    char *readbuf;
    char *arg;
    size_t readbuf_len;
    size_t arg_len;

    fs = fopen(filename, "r");
    if (fs == NULL)
    {
        Error("Response file not found: '%s'", filename);
    }

    // Will be lazily allocated:
    readbuf = NULL; readbuf_len = 0;

    for (;;)
    {
        int c;

        // Skip past any whitespace
        while (!feof(fs))
        {
            c = fgetc(fs);
            if (!isspace(c))
            {
                break;
            }
        }
        if (feof(fs))
        {
            break;
        }

        // At this point we have read the first character of the next argument.
        // An argument consists of a sequence of non-space characters.
        // If we wanted to be especially thoughtful we'd consider quotes so
        // that we can support "long file names", but I'm lazy.
        arg_len = 0;
        while (!feof(fs) && !isspace(c))
        {
            if (arg_len + 1 > readbuf_len)
            {
                readbuf_len += 64;
                readbuf = realloc(readbuf, readbuf_len);
                assert(readbuf != NULL);
            }
            readbuf[arg_len] = c;
            ++arg_len;

            // Next character.
            c = fgetc(fs);
        }

        readbuf[arg_len] = '\0';
        arg = strdup(readbuf);
        assert(arg != NULL);
        result = AppendArgList(result, 1, &arg);
    }

    free(readbuf);
    fclose(fs);

    return result;
}

int ArgListLength(char **args)
{
    int result = 0;
    int i;

    if (args != NULL)
    {
        for (i = 0; args[i] != NULL; ++i)
        {
            ++result;
        }
    }

    return result;
}

// Scan through command line arguments and try to identify the name of the
// .exe that the user is trying to execute. We stop when we either reach an
// argument that is not a flag, or not a flag that we recognize.
static char *IdentifyCallee(int argc, char **argv)
{
    struct flag *f;
    int i;

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            return argv[i];
        }
        f = FindFlagForName(argv[i]);
        if (f == NULL)
        {
            break;
        }
        // Skip over parameter:
        if (f->type != FLAG_BOOL)
        {
            ++i;
        }
    }
    return NULL;
}

static void ReorderArgsForBakedIn(char **args, char *callee)
{
    char **callee_args = NULL;
    char **r, **w;
    struct flag *f;

    // This is to handle a corner case where only flags are baked in,
    // and the user still supplies the name of the callee .exe.
    // Example:
    //   ipxsetup.exe has "-nodes 3 -skill 2" baked in.
    // User invokes:
    //   ipxsetup doom2.exe -warp 10
    // After prepending baked-in args, this will have become:
    //   ipxsetup -nodes 3 -skill 2 doom2.exe -warp 10
    // But we don't know this flag --^^
    // We need to move the .exe to the start so we instead get:
    //   ipxsetup -nodes 3 doom2.exe -skill 2 -warp 10
    if (callee != NULL)
    {
        int i;

        for (i = 1; args[i] != NULL; i++)
        {
            // The use of == rather than strcmp() here is deliberate.
            if (args[i] == callee)
            {
                memmove(&args[2], &args[1], sizeof(char *) * (i - 1));
                args[1] = callee;
                break;
            }
        }
    }

    // We sort through the arguments list and look for flags we recognize.
    // Every argument gets sorted into one of two lists: either the list
    // of arguments for this program (args), or everything else / the program
    // we will invoke (callee_args).
    w = &args[1];
    for (r = &args[1]; *r != NULL; r++)
    {
        f = FindFlagForName(*r);
        if (f != NULL)
        {
            *w = *r;
            ++w;
            if (f->type != FLAG_BOOL && *(r + 1) != NULL)
            {
                ++r;
                *w = *r;
                ++w;
            }
        }
        else
        {
            callee_args = AppendArgList(callee_args, 1, r);
        }
    }

    // `w` now points to the end of the args for this program. Now we can copy
    // back the callee args to the end. The length of `args` never changes.
    memcpy(w, callee_args, sizeof(char *) * ArgListLength(callee_args));
    free(callee_args);
}

static char **AppendBakedInArgs(char **args)
{
    const char *c;

    c = baked_in_config.config;

    while (*c != '\0')
    {
        args = AppendArgList(args, 1, (char **) &c);
        c += strlen(c) + 1;
    }

    return args;
}

static char **ExpandResponseArgs(char **args)
{
    int argc;
    int i;

    argc = ArgListLength(args);

    for (i = 1; i < argc; ++i)
    {
        char **new_args, **response_args;
        int response_args_len;

        if (args[i][0] != '@')
        {
            continue;
        }

        response_args = ReadResponseFile(args[i] + 1);
        response_args_len = ArgListLength(response_args);

        new_args = AppendArgList(NULL, i, args),
        new_args = AppendArgList(
            new_args, response_args_len, response_args),
        new_args = AppendArgList(new_args, argc - i - 1, args + i + 1);

        free(args);
        args = new_args;
        argc += response_args_len - 1;
        i += response_args_len - 1;
    }

    return args;
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

static char **DoParseArgs(int argc, char **argv)
{
    struct flag *f;
    int i;

    if (argc == 1)
    {
        PrintProgramUsage(stdout);
        exit(0);
    }

    StripAPIPointers(&argc, argv);

    for (i = 1; i < argc; ++i)
    {
        if (argv[i][0] != '-')
        {
            return AppendArgList(NULL, argc - i, argv + i);
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
                *f->value.i = MustParseInt(f->name, argv[i + 1],
                                           INT_MIN, INT_MAX);
                break;

            case FLAG_UNSIGNED_INT:
                *f->value.i = MustParseInt(f->name, argv[i + 1], 0, UINT_MAX);
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
}

char **ParseCommandLine(int argc, char **argv)
{
    char **args, **result;
    int i;

    for (i = 1; i < argc; i++)
    {
        if (!strcasecmp(argv[i], "-h") || !strcasecmp(argv[i], "-help")
         || !strcasecmp(argv[i], "--help") || !strcmp(argv[i], "/?"))
        {
            PrintProgramUsage(stdout);
            exit(0);
        }
    }

    args = AppendArgList(NULL, 1, &argv[0]);
    args = AppendBakedInArgs(args);
    args = AppendArgList(args, argc - 1, &argv[1]);
    args = ExpandResponseArgs(args);

    // When we have baked-in arguments, we have to do some rearranging
    // to distinguish between args for this program and the one we invoke.
    if (HAVE_BAKED_IN_CONFIG(baked_in_config))
    {
        ReorderArgsForBakedIn(args, IdentifyCallee(argc, argv));
    }

    result = DoParseArgs(ArgListLength(args), args);

    free(args);

    return result;
}

static char **ExtendArgList(char **args, int *current_argc, int extra_args)
{
    *current_argc = ArgListLength(args);

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

static void DeleteResponseFile(void)
{
    remove(response_file_arg + 1);
    free(response_file_arg);
    response_file_arg = NULL;
}

static void WriteResponseFile(int argc, char **argv)
{
    FILE *fs;
    int i;

    fs = fopen(response_file_arg + 1, "w");

    for (i = 0; i < argc; ++i)
    {
        fprintf(fs, "%s\n", argv[i]);
    }

    fclose(fs);
}

// Workaround for DOS's very restricted command line size limit. If the given
// command line in 'args' would exceed the limit, the list will be truncated,
// the removed options will be moved out to a temporary response file and
// replaced by a reference to the file of the form '@filename.rsp'.
// This works because Doom itself supports response files, and we
// transparently expand response file arguments (ExpandResponseArgs above).
void SquashToResponseFile(char **args)
{
    int len, i;
    int argc;

    len = 0;
    argc = 0;
    for (i = 0; args[i] != NULL; ++i)
    {
        len += 1 + strlen(args[i]);
        ++argc;
    }

    if (len <= MAX_COMMAND_LINE_LEN)
    {
        return;
    }

    // Lazily allocate the response file argument - we'll reuse the same
    // filename for repeated calls and delete the file on exit.
    // Using tmpnam() feels a bit wrong here but DOS is non-multitasking.
    if (response_file_arg == NULL)
    {
        char *name = tmpnam(NULL);
        response_file_arg = malloc(strlen(name) + 2);
        assert(response_file_arg != NULL);
        sprintf(response_file_arg, "@%s", name);
        atexit(DeleteResponseFile);
    }

    // New command line must include response file name:
    len += strlen(response_file_arg) + 1;

    // Remove arguments from the list until we drop below the limit. We
    // try to break where we find a '-' argument so that if we have eg.
    // "-file foo.wad" the whole thing will go into the response file.
    for (i = argc - 1; i > 1; --i)
    {
        len -= strlen(args[i]) + 1;
        if (args[i][0] == '-' && len < MAX_COMMAND_LINE_LEN)
        {
            break;
        }
    }

    // Write response file containing the cut arguments, and truncate the
    // argument list to just include the @filename argument.
    WriteResponseFile(argc - i, args + i);

    args[i] = response_file_arg;
    args[i + 1] = NULL;
}

