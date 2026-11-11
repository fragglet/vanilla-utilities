# sirsetup

This is the manual for the `sirsetup` driver included with Vanilla Utilities.

The `sirsetup` driver allows Doom to be played over a Serial Infrared (SIR)
wireless link. Most laptops in the late 90s shipped with such a port built-in,
usually known as the [IrDA](https://en.wikipedia.org/wiki/IrDA#IrPHY) port.
Serial Infrared provides the physical layer on which the IrDA stack was built,
and `sirsetup` uses SIR without any of the higher level IrDA protocols. In
hardware terms, Serial Infrared is typically exposed as a serial port, and
`sirsetup` will only work with such systems.

Hardware "IrDA dongles" were also sold that plugged into a serial port and
these may also work. However, some dongles required special signalling to be
performed to configure the hardware, which `sirsetup` does not do. Please
report any success or failure you have with such dongles.

## Physical configuration

Because infrared is a shared medium, a transmit can be received by multiple
machines. Up to 8 players are supported in theory, although it's unlikely to
scale to this many in practice.

It is important to carefully arrange all machines that will take part in a
game. There needs to be a clear line-of-sight between the infrared sensors on
all the machines. The typical field of view for the infrared sensor is around
30 degrees. Range is also limited; a distance of more than around 1 meter is
likely to affect reliability.

## Command line arguments

| Argument   | Parameter | Description                          |
| ---------- | --------- | ------------------------------------ |
| -player    | `p`       | Force player `p`. |
| -q         |           | Quiet mode (disable sound when new player found). |
| -nodes     | `n`       | Number of players in game, default 2 |
| -com2      |           | Use COM2 for communication. |
| -com3      |           | Use COM3 for communication. |
| -com4      |           | Use COM4 for communication. |
| -9600 / -14400 / -19200 / -38400 / -57600 / -115200 | | Set baud rate |
| -dup       | `n`       | Reduce movement resolution & bandwidth by factor n |
| -extratics | `n`       | Send n extra tics per packet as insurance |

## Examples

The simplest example establishes a two player game on COM1:
```
C:\DOOM>sirsetup.exe doom.exe
```
To specify a different COM port and baud rate:
```
C:\DOOM>sirsetup.exe -9600 -com2 doom.exe
```
For more than two players, use `-nodes`; for example, for a four player game:
```
C:\DOOM>sirsetup.exe -9600 -com2 -nodes 4 doom.exe
```
