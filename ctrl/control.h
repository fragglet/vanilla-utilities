
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

// Press "Fire".
#define BT_ATTACK       1
// Use button, to open doors, activate switches.
#define BT_USE          2

// Flag: game events, not really buttons.
#define BT_SPECIAL      128
#define BT_SPECIALMASK  3

// Flag, weapon change pending.
// If true, the next 3 bits hold weapon num.
#define BT_CHANGE       4
// The 3bit weapon mask and shift, convenience.
#define BT_WEAPONMASK   (8+16+32)
#define BT_WEAPONSHIFT  3

// Pause the game.
#define BTS_PAUSE     1
// Save the game at each console.
#define BTS_SAVEGAME  2

// Savegame slot numbers
//  occupy the second byte of buttons.
#define BTS_SAVEMASK  (4+8+16)
#define BTS_SAVESHIFT 2

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

typedef void (*control_callback_t)(ticcmd_t *ticcmd);

void ControlRegisterFlags(void);
void ControlLaunchDoom(char **args, control_callback_t callback);
control_handle_t far *ControlGetHandle(long l);
void ControlInvoke(control_handle_t far *handle, ticcmd_t *ticcmd);

#endif                          /* CONTROL_H */
