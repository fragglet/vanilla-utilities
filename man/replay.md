# replay

This is the manual for the `replay` driver included with Vanilla Utilities.

The `replay` driver uses Doom's
[external control API](https://doomwiki.org/wiki/External_control_driver) to
replay demos. The main reason to do this is to continue a demo that was
previously recorded, and record a new demo that is an extension of the original
demo.

## Command line arguments

| Argument   | Parameter  | Description                          |
| ---------- | ---------- | ------------------------------------ |
| -playdemo  | `filename` | Play back the specified demo file.   |
| -strife    |            | Play back a Strife demo.             |

## Examples

To use `replay`, you need to specify a demo file with the `-playdemo` option.
For example:
```
C:\DOOM2>replay.exe -playdemo asdf.lmp doom2.exe
```
(For consistency with Doom, the `.lmp` extension can also be omitted.)

The game will start and you will see the movements from the demo start to
replay. To avoid the game going out of sync, avoid touching the controls until
playback finishes. Once the demo finishes playing back, the game will continue,
and you can use the controls to continue playing the game.

The usual use case for the driver is to record a new demo that continues from
the original demo. For example:
```
C:\DOOM2>replay.exe -playdemo asdf.lmp doom2.exe -record asdf2
```

## Limitations

* `replay` only works with Doom and Strife, because those are the only games
  that support external control drivers. When playing back Strife demos, you
  must specify the `-strife` argument.
* The demo header is ignored, so you must specify any game setup options
  manually (e.g. `-warp`, `-skill`, `-nomonsters`, etc.)
* Multiplayer demos are not supported.
