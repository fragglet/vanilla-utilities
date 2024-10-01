//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/bakedin.h"

static struct baked_in_config config;

// Scan the given file to find the magic string that is included at the
// start of struct baked_in_config. Returns zero if not found.
long FindConfigPosition(FILE *fstream)
{
    char magic[20];
    char buf[64];
    int magic_len;
    int i;

    strcpy(magic, BAKED_IN_MAGIC1);
    strcat(magic, BAKED_IN_MAGIC2);
    magic_len = strlen(magic);

    rewind(fstream);
    memset(buf, 0, sizeof(buf));

    for (;;)
    {
        memmove(buf, buf + magic_len, sizeof(buf) - magic_len);
        if (fread(&buf[sizeof(buf) - magic_len], 1, magic_len,
                  fstream) != magic_len)
        {
            return 0;
        }

        for (i = 0; i < magic_len; i++)
        {
            if (!strncmp(&buf[i], magic, magic_len))
            {
                return ftell(fstream) - sizeof(buf) + i;
            }
        }
    }
    return 0;
}

void ReadConfig(FILE *fstream, long pos)
{
    if (fseek(fstream, pos, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(1);
    }
    if (fread(&config, sizeof(struct baked_in_config), 1, fstream) != 1)
    {
        perror("fread");
        exit(1);
    }
}

void WriteConfig(FILE *fstream, long pos)
{
    if (fseek(fstream, pos, SEEK_SET) != 0)
    {
        perror("fseek");
        exit(1);
    }
    if (fwrite(&config, sizeof(struct baked_in_config), 1, fstream) != 1)
    {
        perror("fwrite");
        exit(1);
    }
}

void PrintConfig(char *filename)
{
    char *c;

    if (!HAVE_BAKED_IN_CONFIG(config))
    {
        printf("%s has no current baked-in arguments.\n", filename);
        return;
    }

    printf("Current baked-in arguments for %s:\n", filename);
    printf("    ");
    for (c = config.config; *c != '\0'; c += strlen(c) + 1)
    {
        printf("%s ", c);
    }
    printf("\n    ");
    for (c = config.config; *c != '\0' || *(c + 1) != '\0'; ++c)
    {
        putchar(*c == '\0' ? ' ' : 0xdf);
    }
    printf("\n");
}

void SetConfig(char **args, int nargs)
{
    char *w;
    size_t total_len = 1;
    int i;

    // As a special case, a single argument of "-" clears all args.
    if (nargs == 1 && !strcmp(args[0], "-"))
    {
        nargs = 0;
    }

    for (i = 0; i < nargs; i++)
    {
        total_len += strlen(args[i]) + 1;
    }
    if (total_len > BAKED_IN_MAX_LEN)
    {
        fprintf(stderr, "Arguments too long to store in executable: %d > %d\n",
                total_len, BAKED_IN_MAX_LEN);
        exit(1);
    }

    w = config.config;
    for (i = 0; i < nargs; i++)
    {
        strcpy(w, args[i]);
        w += strlen(args[i]) + 1;
    }
    *w = '\0';
}

void PrintHelpText(char *cmdname)
{
    printf(
        "Set baked-in command line arguments for Vanilla Utilities binary.\n"
        "\n"
        "Usage:\n"
        "    %s filename.exe         - Show current baked-in arguments.\n"
        "    %s filename.exe <args>  - Set baked-in arguments.\n"
        "    %s filename.exe -       - Clear baked-in arguments.\n"
        "\n"
        "Examples:\n"
        "    %s vcommit.exe duke3d.exe     :: Always run duke3d.exe\n"
        "    %s vrottcom.exe rott.exe      :: Always run rott.exe\n"
        "    %s sersetup.exe doom2.exe     :: Always run doom2.exe\n"
        "\n"
        "    :: Make a copy of ipxsetup that always starts a 3-player game:\n"
        "    COPY ipxsetup.exe 3player.exe\n"
        "    %s 3player.exe -nodes 3\n"
        "\n",
        cmdname, cmdname, cmdname, cmdname, cmdname, cmdname, cmdname
    );
}

enum command {
    CMD_PRINT,
    CMD_SET,
};

int main(int argc, char *argv[])
{
    FILE *fstream;
    enum command cmd;
    long config_pos;

    switch (argc)
    {
        case 1:
            PrintHelpText(argv[0]);
            exit(0);
            break;

        case 2:
            cmd = CMD_PRINT;
            break;

        default:
            cmd = CMD_SET;
            break;
    }

    fstream = fopen(argv[1], cmd == CMD_PRINT ? "rb" : "r+b");
    if (fstream == NULL)
    {
        perror("fopen");
        exit(1);
    }

    config_pos = FindConfigPosition(fstream);
    if (config_pos == 0)
    {
        fprintf(stderr, "Not a vanilla-utilities binary? Failed to find "
                        "magic string.\n");
        exit(1);
    }

    ReadConfig(fstream, config_pos);

    switch (cmd)
    {
        case CMD_PRINT:
            PrintConfig(argv[1]);
            break;

        case CMD_SET:
            SetConfig(argv + 2, argc - 2);
            WriteConfig(fstream, config_pos);
            printf("Baked-in arguments updated for %s.\n", argv[1]);
            break;
    }

    fclose(fstream);

    return 0;
}

