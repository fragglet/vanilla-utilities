//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#include <conio.h>
#include <dos.h>

#if defined(__TURBOC__)
#define cmdline_argc  _argc
#define cmdline_argv  _argv
#define __stdcall
typedef int ssize_t;
#else
#define cmdline_argc  __argc
#define cmdline_argv  __argv
#endif

#define strcasecmp stricmp
#define strncasecmp strnicmp

#define INPUT( port )        inp( port )
#define OUTPUT( port, data ) (void) outp( port, data )

long GetEntropy(void);

void far_memcpy(void far *dest, void far *src, size_t nbytes);
int far_memcmp(void far *a, void far *b, size_t nbytes);
void far_memmove(void far *dest, void far *src, size_t nbytes);
void far_bzero(void far *dest, size_t nbytes);

