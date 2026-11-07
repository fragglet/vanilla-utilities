# statdump

This is the manual for the `statdump` driver included with Vanilla Utilities.

`statdump` is an implementation of a Doom
[external statistics driver](https://doomwiki.org/wiki/Statistics_driver). DOS
Doom has an obscure command line argument named `-statcopy`; when used
correctly, the game copies statistics information into an external memory
buffer at the end of each level. This includes such information as the number
of monsters killed, number of items collected and secrets discovered. It is the
same information used to construct the intermission screen shown to the player.

## Command line arguments

| Argument   | Parameter  | Description                                 |
| ---------- | ---------- | ------------------------------------------- |
| -o         | `filename` | Write captured statistics to the given file |

## Examples

The simplest example runs `doom2.exe` and outputs the statistics information
to the screen when the game exits:
```
C:\DOOM2>statdump.exe doom2.exe
```
Often, it's preferable to save the statistics to a file instead using `-o`.
For example:
```
C:\DOOM2>statdump.exe -o stats.txt doom2.exe
```
To play back a demo and save statistics:
```
C:\DOOM2>statdump.exe -o 30nm4949.txt doom2.exe -timedemo 30nm4949 -nodraw -noblit
```

## Limitations

 * `statdump` only works with Doom, not with any other Doom engine games.
 * The driver cannot always determine whether Doom or Doom II is being played.
   It can sometimes be discovered heuristically (for example, if the player
   plays a level higher than MAP09, or on an episode other than episode 1). If
   the game cannot be determined, levels in the form "E1M3 / MAP03".
 * Statistics are not saved on the last level (whenever the intermission screen
   is not displayed). These are E1M8, E2M8, E3M8, E4M8 and MAP30.
 * Statistics are saved for a maximum of 32 levels. This is enough for a full
   playthrough of Doom II including secret levels, but if you restart the game
   and complete the same level multiple times, statistics may be lost.
 * A table of frags is generated if there is more than one player in the game.
   The driver cannot tell if the game is running in cooperative or deathmatch
   mode.

## Example output

Normal single player output looks like this:
```
===========================================
E1M1 / MAP01
===========================================

Time: 0:29 (par: 0:30)

Player 1 (Green):
	Kills: 29 / 29 (100%)
	Items: 3 / 38 (7%)
	Secrets: 3 / 3 (100%)
```
In multiplayer mode, a table of frags is also produced:
```
===========================================
MAP17
===========================================

Time: 9:08 (par: 7:00)

Player 1 (Green):
	Kills: 25 / 126 (19%)
	Items: 36 / 112 (32%)
	Secrets: 0 / 3 (0%)
Player 2 (Indigo):
	Kills: 95 / 126 (75%)
	Items: 74 / 112 (66%)
	Secrets: 3 / 3 (100%)
Frags:
		Green	Indigo	
		-------------------------------- VICTIMS
	Green	|0	0	
	Indigo	|0	0	
		|
	     KILLERS
```
