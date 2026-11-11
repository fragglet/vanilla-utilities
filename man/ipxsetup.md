# ipxsetup

This is the manual for the `ipxsetup` driver included with Vanilla Utilities.

The `ipxsetup` driver allows Doom to be played over the
[IPX protocol](https://en.wikipedia.org/wiki/Internetwork_Packet_Exchange)
created by Novell Inc. The driver originally shipped with DOS Doom,
starting with v1.2 of the game; versions also shipped with Heretic, Hexen
and Strife. This version of `ipxsetup` is compatible with the original
versions, but includes some improvements.

## Command line arguments

| Argument   | Parameter | Description                          |
| ---------- | --------- | ------------------------------------ |
| -nodes     | `n`       | Number of players in game, default 2. |
| -player    | `p`       | Force this to be player #p. |
| -q         |           | Quiet mode (disable sound when new player found). |
| -socket    | `socket`  | Use alternate IPX socket number, default 0x869b (34459). |
| -dup       | `n`       | Reduce movement resolution & bandwidth by factor n. |
| -extratics | `n`       | Send n extra tics per packet as insurance. |

## Examples

To use the driver, you must have the Novell Netware client drivers installed;
specifically the `IPXODI` driver. `ipxsetup` will not start if this driver is
not installed. Under Windows 9x, you can also use the Microsoft-provided IPX
driver.

Starting a two player game is the simplest use case. To launch a two player
game of Doom II in cooperative mode, type this on both machines:
```
C:\DOOM2>ipxsetup.exe doom2.exe
```
For more than two players, use `-nodes`. This launches a three player
deathmatch game:
```
C:\DOOM2>ipxsetup.exe -nodes 3 doom2.exe -deathmatch
```
By default the player numbers are determined based on network address. To
force a particular machine to be a particular player number, use `-player`.
For example, to force the machine to be player 2 (indigo), use:
```
C:\DOOM2>ipxsetup.exe -nodes 3 -player 2 doom2.exe -deathmatch
```

## Notes

* This version of `ipxsetup` supports the "xttl" protocol extensions. These
  automatically synchronize the `-dup` parameter and allow the player number
  to be specified using `-player`.
* The "official" IPX socket number that was registered with Novell by id
  Software was 0x869c (34460), but due to an integer overflow bug, the driver
  used 0x869b (34459) as its default instead. This version of the driver
  preserves that default for backwards compatibility reasons.
