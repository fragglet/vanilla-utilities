
 /* 
 
 Copyright(C) 2007,2011 Simon Howard

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

 Functions for presenting the information captured from the statistics
 buffer to a file.

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stats.h"
#include "statprnt.h"

#define TICRATE 35

typedef enum
{
    doom1,
    doom2,
    indetermined
} GameMode_t;

/* Par times for E1M1-E1M9. */
static const int doom1_par_times[] =
{
    30, 75, 120, 90, 165, 180, 180, 30, 165,
};

/* Par times for MAP01-MAP09. */
static const int doom2_par_times[] =
{
    30, 90, 120, 120, 90, 150, 120, 120, 270,
};

/* Player colors. */
static const char *player_colors[] =
{
    "Green", "Indigo", "Brown", "Red"
};

static GameMode_t gamemode = indetermined;

/* Try to work out whether this is a Doom 1 or Doom 2 game, by looking 
 * at the episode and map, and the par times.  This is used to decide
 * how to format the level name.  Unfortunately, in some cases it is
 * impossible to determine whether this is Doom 1 or Doom 2. */

void discover_gamemode(wbstartstruct_t *stats, int num_stats)
{
    int partime;
    int level;
    int i;

    if (gamemode != indetermined)
    {
        return;
    }

    for (i=0; i<num_stats; ++i)
    {
        level = stats[i].last;

        /* If episode 2, 3 or 4, this is Doom 1. */

        if (stats[i].epsd > 0)
        {
            gamemode = doom1;
            return;
        }

        /* This is episode 1.  If this is level 10 or higher,
           it must be Doom 2. */

        if (level >= 9)
        {
            gamemode = doom2;
            return;
        }

        /* Try to work out if this is Doom 1 or Doom 2 by looking
           at the par time. */

        partime = stats[i].partime;

        if (partime == doom1_par_times[level] * TICRATE
         && partime != doom2_par_times[level] * TICRATE)
        {
            gamemode = doom1;
            return;
        }

        if (partime != doom1_par_times[level] * TICRATE
         && partime == doom2_par_times[level] * TICRATE)
        {
            gamemode = doom2;
            return;
        }
    }
}

/* Returns the number of players active in the given stats buffer. */

static int get_num_players(wbstartstruct_t *stats)
{
    int i;
    int num_players = 0;

    for (i=0; i<MAXPLAYERS; ++i)
    {
        if (stats->plyr[i].in)
        {
            ++num_players;
        }
    }

    return num_players;
}

static void print_banner(FILE *stream)
{
    fprintf(stream, "===========================================\n");
}

static void print_percentage(FILE *stream, int amount, int total)
{
    if (total == 0)
    {
        fprintf(stream, "0");
    }
    else
    {
        fprintf(stream, "%i / %i", amount, total);

        fprintf(stream, " (%i%%)", (amount * 100) / total);
    }
}

/* Display statistics for a single player. */

static void print_player_stats(FILE *stream, wbstartstruct_t *stats,
        int player_num)
{
    wbplayerstruct_t *player = &stats->plyr[player_num];

    fprintf(stream, "Player %i (%s):\n", player_num + 1,
            player_colors[player_num]);

    /* Kills percentage */

    fprintf(stream, "\tKills: ");
    print_percentage(stream, player->skills, stats->maxkills);
    fprintf(stream, "\n");

    /* Items percentage */

    fprintf(stream, "\tItems: ");
    print_percentage(stream, player->sitems, stats->maxitems);
    fprintf(stream, "\n");

    /* Secrets percentage */

    fprintf(stream, "\tSecrets: ");
    print_percentage(stream, player->ssecret, stats->maxsecret);
    fprintf(stream, "\n");
}

/* Frags table for multiplayer games. */

static void print_frags_table(FILE *stream, wbstartstruct_t *stats)
{
    int x, y;

    fprintf(stream, "Frags:\n");

    /* Print header */

    fprintf(stream, "\t\t");

    for (x=0; x<MAXPLAYERS; ++x)
    {

        if (!stats->plyr[x].in)
        {
            continue;
        }

        fprintf(stream, "%s\t", player_colors[x]);
    }

    fprintf(stream, "\n");

    fprintf(stream, "\t\t-------------------------------- VICTIMS\n");

    /* Print table */

    for (y=0; y<MAXPLAYERS; ++y)
    {
        if (!stats->plyr[y].in)
        {
            continue;
        }

        fprintf(stream, "\t%s\t|", player_colors[y]);

        for (x=0; x<MAXPLAYERS; ++x)
        {
            if (!stats->plyr[x].in)
            {
                continue;
            }

            fprintf(stream, "%i\t", stats->plyr[y].frags[x]);
        }

        fprintf(stream, "\n");
    }

    fprintf(stream, "\t\t|\n");
    fprintf(stream, "\t     KILLERS\n");
}

/* Displays the level name: MAPxy or ExMy, depending on game mode. */

static void print_level_name(FILE *stream, int episode, int level)
{
    print_banner(stream);

    switch (gamemode)
    {

        case doom1:
            fprintf(stream, "E%iM%i\n", episode + 1, level + 1);
            break;
        case doom2:
            fprintf(stream, "MAP%02i\n", level + 1);
            break;
        case indetermined:
            fprintf(stream, "E%iM%i / MAP%02i\n", 
                    episode + 1, level + 1, level + 1);
            break;
    }

    print_banner(stream);
}

/* Print details of a statistics buffer to the given file. */

void print_stats(FILE *stream, wbstartstruct_t *stats)
{
    int leveltime, partime;
    int i;

    print_level_name(stream, stats->epsd, stats->last);
    fprintf(stream, "\n");

    leveltime = stats->plyr[0].stime / TICRATE;
    partime = stats->partime / TICRATE;
    fprintf(stream, "Time: %i:%02i", leveltime / 60, leveltime % 60);
    fprintf(stream, " (par: %i:%02i)\n", partime / 60, partime % 60);
    fprintf(stream, "\n");

    for (i=0; i<MAXPLAYERS; ++i)
    {
        if (stats->plyr[i].in)
        {
            print_player_stats(stream, stats, i);
        }
    }

    if (get_num_players(stats) >= 2)
    {
        print_frags_table(stream, stats);
    }

    fprintf(stream, "\n");
}

