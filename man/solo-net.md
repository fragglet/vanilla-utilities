# solo-net

This is the manual for the `solo-net` driver included with Vanilla Utilities.

The `solo-net` driver is a minimalist "null" network driver that simulates the
`-solo-net` option supported by several Doom source ports. The typical use
case (implied by the name) is to start a network game with only one player.
This is different from a normal single player game, as Doom spawns different
in-game items in multiplayer mode.

## Command line arguments

| Argument   | Parameter | Description                          |
| ---------- | --------- | ------------------------------------ |
| -nodes     | `n`       | Number of players in game, default 1 |
| -dup       | `n`       | Reduce movement resolution & bandwidth by factor n |
| -extratics | `n`       | Send n extra tics per packet as insurance |

## Examples

The `solo-net` driver has very few options. The simplest example starts a
single player multiplayer game:
```
C:\DOOM2>solo-net.exe doom2.exe
```
Additional players can be simulated with the `-nodes` option. For example, to
simulate a three player deathmatch game:
```
C:\DOOM2>solo-net.exe -nodes 3 doom2.exe -deathmatch
```
However, it should be noted that using this feature is fragile, because the
driver cannot correctly predict the consistency values used to check if the
game has gone out of sync. In practice, this means that the game will abruptly
exit if the in-game avatars for the other players are moved (e.g. by shooting
them).
