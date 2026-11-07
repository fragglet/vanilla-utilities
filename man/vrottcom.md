# vrottcom

This is the manual for the `vrottcom` driver included with Vanilla Utilities.

The `vrottcom` driver allows Doom network drivers to be used with the ROTTCOM
interface supported by *Rise of the Triad*.

## Command line arguments

| Argument        | Description            |
| --------------- | ---------------------- |
| -remoteridicule | Enable remote ridicule |

## Examples

The `vrottcom` driver has no special command line options of its own. It must be
invoke it from a Doom network driver, and it then invokes the game.

The following simple example uses the `parsetup` driver to start a Rise of the
Triad multiplayer game over a parallel port connection (Laplink cable):
```
C:\ROTT>parsetup.exe vrottcom.exe rott.exe
```
`vrottcom` can be used with the `udpsetup` driver to play *Rise of the Triad*
multiplayer over the Internet.

For example, to create the server:
```
C:\ROTT>udpsetup -s -nodes 4 vrottcom rott
```
and from the clients (here, `10.0.3.4` is the IP address of the server):
```
C:\ROTT>udpsetup -c 10.0.3.4 -nodes 4 vrottcom rott
```
See `udpsetup.txt` for more information about this driver.
