//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

// Functions for presenting the information captured from the statistics
// buffer to a file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stat/stats.h"
#include "stat/statprnt.h"

#define TICRATE 35

typedef enum {
    doom1,
    doom2,
    indetermined
} GameMode_t;

/* Par times for E1M1-E1M9. */
static const int doom1_par_times[] = {
    30, 75, 120, 90, 165, 180, 180, 30, 165,
};

/* Par times for MAP01-MAP09. */
static const int doom2_par_times[] = {
    30, 90, 120, 120, 90, 150, 120, 120, 270,
};

/* Player colors. */
static const char *player_colors[] = {
    "Green", "Indigo", "Brown", "Red"
};

static GameMode_t gamemode = indetermined;

/* Try to work out whether this is a Doom 1 or Doom 2 game, by looking 
 * at the episode and map, and the par times.  This is used to decide
 * how to format the level name.  Unfortunately, in some cases it is
 * impossible to determine whether this is Doom 1 or Doom 2. */

void DiscoverGamemode(wbstartstruct_t *stats, int num_stats)
{
    int partime;
    int level;
    int i;

    if (gamemode != indetermined)
    {
        return;
    }

    for (i = 0; i < num_stats; ++i)
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

static int GetNumPlayers(wbstartstruct_t *stats)
{
    int i;
    int num_players = 0;

    for (i = 0; i < DOOM_MAXPLAYERS; ++i)
    {
        if (stats->plyr[i].in)
        {
            ++num_players;
        }
    }

    return num_players;
}

static void PrintBanner(FILE *stream)
{
    fprintf(stream, "===========================================\n");
}

static void PrintPercentage(FILE *stream, int amount, int total)
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

static void PrintPlayerStats(FILE *stream, wbstartstruct_t *stats,
                               int player_num)
{
    wbplayerstruct_t *player = &stats->plyr[player_num];

    fprintf(stream, "Player %i (%s):\n", player_num + 1,
            player_colors[player_num]);

    /* Kills percentage */

    fprintf(stream, "\tKills: ");
    PrintPercentage(stream, player->skills, stats->maxkills);
    fprintf(stream, "\n");

    /* Items percentage */

    fprintf(stream, "\tItems: ");
    PrintPercentage(stream, player->sitems, stats->maxitems);
    fprintf(stream, "\n");

    /* Secrets percentage */

    fprintf(stream, "\tSecrets: ");
    PrintPercentage(stream, player->ssecret, stats->maxsecret);
    fprintf(stream, "\n");
}

/* Frags table for multiplayer games. */

static void PrintFragsTable(FILE *stream, wbstartstruct_t *stats)
{
    int x, y;

    fprintf(stream, "Frags:\n");

    /* Print header */

    fprintf(stream, "\t\t");

    for (x = 0; x < DOOM_MAXPLAYERS; ++x)
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

    for (y = 0; y < DOOM_MAXPLAYERS; ++y)
    {
        if (!stats->plyr[y].in)
        {
            continue;
        }

        fprintf(stream, "\t%s\t|", player_colors[y]);

        for (x = 0; x < DOOM_MAXPLAYERS; ++x)
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

static void PrintLevelName(FILE *stream, int episode, int level)
{
    PrintBanner(stream);

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

    PrintBanner(stream);
}

/* Print details of a statistics buffer to the given file. */

void PrintStats(FILE *stream, wbstartstruct_t *stats)
{
    int leveltime, partime;
    int i;

    PrintLevelName(stream, stats->epsd, stats->last);
    fprintf(stream, "\n");

    leveltime = stats->plyr[0].stime / TICRATE;
    partime = stats->partime / TICRATE;
    fprintf(stream, "Time: %i:%02i", leveltime / 60, leveltime % 60);
    fprintf(stream, " (par: %i:%02i)\n", partime / 60, partime % 60);
    fprintf(stream, "\n");

    for (i = 0; i < DOOM_MAXPLAYERS; ++i)
    {
        if (stats->plyr[i].in)
        {
            PrintPlayerStats(stream, stats, i);
        }
    }

    if (GetNumPlayers(stats) >= 2)
    {
        PrintFragsTable(stream, stats);
    }

    fprintf(stream, "\n");
}
