
 /* 

    Copyright (C) 1993-1996 by id Software, Inc.
    Copyright (C) 2007 Simon Howard

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

    Structure copied by Doom when using the -statcopy option.  From the
    Doom source.

  */

#ifndef STATS_H
#define STATS_H

#define DOOM_MAXPLAYERS 4

typedef struct {
    long in;                    /* whether the player is in game */

    /* Player stats, kills, collected items etc. */
    long skills;
    long sitems;
    long ssecret;
    long stime;
    long frags[DOOM_MAXPLAYERS];
    long score;                 /* current score on entry, modified on return */

} wbplayerstruct_t;

typedef struct {
    long epsd;                  /* episode # (0-2) */

    /* if true, splash the secret level */
    long didsecret;

    /* previous and next levels, origin 0 */
    long last;
    long next;

    long maxkills;
    long maxitems;
    long maxsecret;
    long maxfrags;

    /* the par time */
    long partime;

    /* index of this player in game */
    long pnum;

    wbplayerstruct_t plyr[DOOM_MAXPLAYERS];

} wbstartstruct_t;

typedef void (*stats_callback_t)(wbstartstruct_t *stats, void *user_data);

void StatsLaunchDoom(char **args, stats_callback_t callback, void *user_data);

#endif
