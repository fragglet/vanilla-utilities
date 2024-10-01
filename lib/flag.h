//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

typedef void (*api_pointer_callback_t)(long value);

void SetHelpText(char *program_description, char *example_cmd);
void BoolFlag(const char *name, int *ptr, const char *help_text);
void IntFlag(const char *name, int *ptr,
             const char *param_name, const char *help_text);
void UnsignedIntFlag(const char *name, unsigned int *ptr,
                     const char *param_name, const char *help_text);
void StringFlag(const char *name, char **ptr,
             const char *param_name, const char *help_text);
void APIPointerFlag(const char *name, api_pointer_callback_t callback);
void PrintProgramUsage(FILE *output);
char **ParseCommandLine(int argc, char **argv);
char **AppendArgList(char **args, int argc, char **argv);
char **AppendArgs(char **args, ...);
int ArgListLength(char **args);
void SquashToResponseFile(char **args);

#define DuplicateArgList(args) \
    AppendArgList(NULL, ArgListLength(args), args)

