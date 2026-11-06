# parsetup

This is the manual for the `parsetup` driver included with Vanilla Utilities.

The `parsetup` driver allows Doom to be played over a parallel port,
using a ["LapLink cable"](https://en.wikipedia.org/wiki/Laplink#Cable) to
connect two machines. This driver is based on
[the driver released by Scott Coleman](https://www.doomworld.com/idgames/utils/serial/psetup11)
in 1995.

## Command line arguments

| Argument   | Parameter | Description                          |
| ---------- | --------- | ------------------------------------ |
| -player1   |           | Force player 1. |
| -player2   |           | Force player 2. |
| -lpt2      |           | Use the LPT2 port instead of LPT1. |
| -lpt3      |           | Use the LPT3 port instead of LPT1. You must also specify IRQ using `-irq`. |
| -port      |           | Manually specify the I/O port for the parallel port. |
| -irq       |           | Manually specify the IRQ channel for the parallel port. |
| -dup       | <n>       | Reduce movement resolution & bandwidth by factor n. |
| -extratics | <n>       | Send n extra tics per packet as insurance. |

## Examples

The simplest example establishes a connection on LPT1:
```
C:\DOOM>parsetup.exe doom.exe
```
To specify a different port, eg. LPT2:
```
C:\DOOM>parsetup.exe -lpt2 doom.exe
```
The player numbers are randomly chosen. To force a particular machine to be
a particular player, use `-player1` or `-player2`. For example, to force this
machine to be the indigo player:
```
C:\DOOM>parsetup.exe -player2 doom.exe
```
It is not necessary for both machines to use the `-player1` / `-player2`
arguments.
