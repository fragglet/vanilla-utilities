
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

typedef unsigned char byte;

typedef struct
{
    signed char forwardmove;    // *2048 for move
    signed char sidemove;       // *2048 for move
    short       angleturn;      // <<16 for angle delta
    short       consistancy;    // netgame check
    byte        chatchar;
    byte        buttons;
    byte        buttons2;
    long        inventoryitem;
} ticcmd_t;

typedef struct
{
    char *name;
    int args;
    void (*callback)(char *args[]);
} control_param_t;

typedef void (*control_callback_t)(ticcmd_t *ticcmd, void *user_data);

int control_parse_cmd_line(int argc, char *argv[], control_param_t *params);
void control_launch_doom(char **extra_args, control_callback_t callback,
                         void *user_data);

#endif /* CONTROL_H */

