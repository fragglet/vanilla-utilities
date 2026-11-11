//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#ifndef VUTILS_LIB_DOS_H
#define VUTILS_LIB_DOS_H

#include <conio.h>
#include <dos.h>

#if defined(__TURBOC__)
#define cmdline_argc _argc
#define cmdline_argv _argv
#define __stdcall
typedef int ssize_t;
#else
#define cmdline_argc __argc
#define cmdline_argv __argv
#endif

#define strcasecmp  stricmp
#define strncasecmp strnicmp

#define INPUT(port)        inp(port)
#define OUTPUT(port, data) (void) outp(port, data)

#define LED_SCROLL_LOCK 0x01
#define LED_NUM_LOCK    0x02
#define LED_CAPS_LOCK   0x04

int SetKeyboardLEDs(int value);
long GetEntropy(void);

void far_memcpy(void far *dest, void far *src, size_t nbytes);
int far_memcmp(void far *a, void far *b, size_t nbytes);
void far_memmove(void far *dest, void far *src, size_t nbytes);
void far_bzero(void far *dest, size_t nbytes);

#endif /* #ifndef VUTILS_LIB_DOS_H */
