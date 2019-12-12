
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

#ifndef CONTROL_H
#define CONTROL_H

#include "lib/inttypes.h"

typedef struct control_handle_s control_handle_t;

typedef struct {
    signed char forwardmove;    // *2048 for move
    signed char sidemove;       // *2048 for move
    int16_t angleturn;            // <<16 for angle delta
    int16_t consistancy;          // netgame check
    uint8_t chatchar;
    uint8_t buttons;
    uint8_t buttons2;
    int32_t inventoryitem;
} ticcmd_t;

typedef void (*control_callback_t)(ticcmd_t *ticcmd, void *user_data);

void ControlRegisterFlags(void);
void ControlLaunchDoom(char **args, control_callback_t callback,
                       void *user_data);
control_handle_t far *ControlGetHandle(char **args);
void ControlInvoke(control_handle_t far *handle, ticcmd_t *ticcmd);

#endif                          /* CONTROL_H */
