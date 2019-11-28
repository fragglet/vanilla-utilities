
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

    Functions for interacting with the Doom -control API.

  */

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <process.h>

#include "control.h"

typedef struct {
    long intnum;
    ticcmd_t ticcmd;
} control_buf_t;

static control_buf_t control_buf;

static control_callback_t int_callback;
static void *int_user_data;

static int doom_argc;
static char **doom_argv;

static int force_vector = 0;
static void interrupt(*old_isr) ();

// Interrupt service routine.

static void interrupt control_isr()
{
    memset(&control_buf.ticcmd, 0, sizeof(ticcmd_t));
    int_callback(&control_buf.ticcmd, int_user_data);
}

// Check if an interrupt vector is available.

static int interrupt_vector_available(int intnum)
{
    unsigned char far *vector;

    vector = *((char far * far *)(intnum * 4));

    return vector == NULL || *vector == 0xcf;
}

// Find what interrupt number to use.

static int find_interrupt_num(void)
{
    int intnum;
    int i;

    // -cvector specified in parameters?

    if (force_vector)
    {
        if (interrupt_vector_available(force_vector))
        {
            return force_vector;
        }
        else
        {
            fprintf(stderr, "Interrupt vector 0x%x not available!\n",
                    force_vector);
            exit(-1);
        }
    }

    // Figure out a vector to use automatically.

    for (i = 0x60; i <= 0x66; ++i)
    {
        if (interrupt_vector_available(i))
        {
            return i;
        }
    }

    fprintf(stderr, "Failed to find an available interrupt vector!\n");
    exit(-1);

    return -1;
}

// Install the interrupt handler on the specified interrupt vector.

static void hook_interrupt_handler(int intnum)
{
    void interrupt(*isr) ();

    old_isr = getvect(intnum);

    isr = MK_FP(_CS, (int)control_isr);
    setvect(intnum, isr);
}

// Unload the interrupt handler.

static void restore_interrupt_handler(int intnum)
{
    setvect(intnum, old_isr);
}

// Look up the specified parameter in the given list of supported parameters.

static control_param_t *lookup_param(control_param_t *params, char *arg)
{
    int i;

    if (params == NULL)
    {
        return NULL;
    }

    for (i = 0; params[i].name != NULL; ++i)
    {
        if (!strcmp(arg, params[i].name))
        {
            return &params[i];
        }
    }

    return NULL;
}

// Parse the command line arguments. This must be called before
// control_launch_doom. Returns non-zero for success.

int control_parse_cmd_line(int argc, char *argv[], control_param_t *params)
{
    control_param_t *param;
    int i;

    for (i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "-cvector"))
        {
            if (i + 1 >= argc)
            {
                return 0;
            }

            sscanf(argv[i + 1], "0x%x", &force_vector);
            ++i;
            continue;
        }

        // Custom parameter?

        param = lookup_param(params, argv[i]);

        if (param != NULL)
        {
            if (i + param->args >= argc)
            {
                return 0;
            }

            param->callback(argv + i + 1);
            i += param->args;
            continue;
        }

        // Can't end on an option.

        if (argv[i][0] == '-')
        {
            return 0;
        }

        // Not a custom parameter - start of command line. Save and return.

        doom_argc = argc - i;
        doom_argv = argv + i;
        return 1;
    }

    return 0;
}

static int count_extra_args(char **extra_args)
{
    int i;

    if (extra_args == NULL)
    {
        return 0;
    }

    for (i = 0; extra_args[i] != NULL; ++i) ;

    return i;
}

// Launch the game. control_parse_cmd_line must have been called first.

void control_launch_doom(char **extra_args, control_callback_t callback,
                         void *user_data)
{
    int actual_argc;
    char **actual_argv;
    char addr_string[32];
    long flataddr;
    int intnum;
    int num_extra_args;

    intnum = find_interrupt_num();
    printf("Control API: Using interrupt vector 0x%x\n", intnum);

    // Initialise the interrupt handler.

    int_callback = callback;
    int_user_data = user_data;
    control_buf.intnum = intnum;
    hook_interrupt_handler(intnum);

    // Build the command line arguments.

    num_extra_args = count_extra_args(extra_args);
    actual_argv = malloc((doom_argc + num_extra_args + 3) * sizeof(char *));

    memcpy(actual_argv, doom_argv, doom_argc * sizeof(char *));
    actual_argc = doom_argc;

    // Add extra args:

    memcpy(actual_argv + actual_argc, extra_args,
           num_extra_args * sizeof(char *));
    actual_argc += num_extra_args;

    // Add the -control argument.

    actual_argv[actual_argc] = "-control";
    flataddr = (long)_DS *16 + (unsigned)(&control_buf);
    sprintf(addr_string, "%li", flataddr);
    actual_argv[actual_argc + 1] = addr_string;
    actual_argc += 2;

    {
        int i;

        for (i = 0; i < actual_argc; ++i)
            printf("%i: %s\n", i, actual_argv[i]);
    }

    // Terminating NULL.

    actual_argv[actual_argc] = NULL;

    // Launch Doom:
    spawnv(P_WAIT, actual_argv[0], actual_argv);

    free(actual_argv);

    restore_interrupt_handler(intnum);
}
