# sersetup

This is the manual for the `sersetup` driver included with Vanilla Utilities.

The `sersetup` driver allows Doom to be played over a dial-up modem, or over
a [null-modem cable](https://en.wikipedia.org/wiki/Null_modem). The driver
originally shipped with DOS Doom, starting with v1.2 of the game; versions also
shipped with Heretic, Hexen and Strife. This version of `sersetup` is
compatible with the original versions, but includes some improvements.

The driver has three different modes:

 * **Null modem connection**: This is the default, used for directly
   connecting two computers via a cable connected to their serial ports.
 * **Answer call**: This mode is used if the `-answer` command-line argument
   is given. The driver talks to a modem on the given serial port, waits for an
   incoming phone call to be received, and answers the call.
 * **Dial modem**: This mode is used if the `-dial` command-line argument is
   given. The driver talks to a modem on the given serial port, dials a number
   and establishes a link to the driver on the other end.

If you are using a modem, you should provide a configuration file. The driver
ships with configuration files for a wide variety of modems from different
manufacturers. Copy the appropriate .cfg file and rename it to `modem.cfg`.

## Command line arguments

| Argument   | Parameter | Description                          |
| ---------- | --------- | ------------------------------------ |
| -dial      | `phone#`  | Dial the given phone number |
| -answer    |           | Listen for incoming call |
| -bg        |           | Answer call in the background |
| -player1   |           | Force player 1 |
| -player2   |           | Force player 2 |
| -modemcfg  | `f.cfg`   | Specify config file for modem |
| -com2      |           | Use COM2 for communication |
| -com3      |           | Use COM3 for communication |
| -com4      |           | Use COM4 for communication |
| -9600 / -14400 / -19200 / -38400 / -57600 / -115200 | | Set baud rate |
| -dup       | `n`       | Reduce movement resolution & bandwidth by factor n |
| -extratics | `n`       | Send n extra tics per packet as insurance |

## Examples

The simplest example establishes a null-modem connection on COM1:
```
C:\DOOM>sersetup.exe doom.exe
```
To specify a different COM port and baud rate:
```
C:\DOOM>sersetup.exe -115200 -com2 doom.exe
```
To listen for an incoming phone call, use `-answer` (this establishes a
deathmatch game using a modem on COM2):
```
C:\DOOM>sersetup.exe -answer -com2 doom.exe -deathmatch
```
To dial another machine that is waiting for a call:
```
C:\DOOM>sersetup.exe -dial 555-1212 doom.exe -deathmatch
```
The player numbers are randomly chosen. To force a particular machine to be
a particular player, use `-player1` or `-player2`. For example, to force this
machine to be the indigo player:
```
C:\DOOM>sersetup.exe -player2 doom.exe
```
It is not necessary for both machines to use the `-player1` / `-player2`
arguments.

## Enhancements

* This version of the driver supports background answering. If the `-bg`
  argument is given, the game will launch immediately rather than waiting
  for the incoming call. This allows the game's slow `R_Init` sequence to
  complete before the call is answered. Background answering implies
  `-player1`, unless `-player2` is given.
