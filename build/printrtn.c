//
// Copyright(C) 2026 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

// This is a tiny program that just prints the exit code of the last
// command that ran.

#include <assert.h>
#include <dos.h>
#include <process.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
    union REGS regs;

    // Get exit code of last command:
    regs.h.ah = 0x4d;
    int86(0x21, &regs, &regs);

    printf("%d\n", regs.h.al);

    return 0;
}
