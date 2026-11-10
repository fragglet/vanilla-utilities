# vcommit

This is the manual for the `vcommit` driver included with Vanilla Utilities.

The `vcommit` driver allows Doom network drivers to be used with BUILD engine
games that support the COMMIT network interface. These include *Duke Nukem 3D*,
*Blood*, *Shadow Warrior*, *Redneck Rampage* and others.

## Command line arguments

The driver has no command line options.

## Examples

The `vcommit` driver has no special command line options of its own. It must be
invoked from a Doom network driver, and it then invokes the game.

The following simple example uses the `parsetup` driver to play Duke Nukem 3D
over a parallel port connection (Laplink cable):
```
C:\DUKE3D>parsetup.exe vcommit.exe duke3d.exe
```
The `vcommit` adapter can be used with the `udpsetup` driver to play
multiplayer games over the Internet. A demo video [can be found
here](https://youtu.be/4L5wVLp5wVE).

For example, to create the server:
```
C:\DUKE3D>udpsetup -s -nodes 4 vcommit duke3d
```
and from the clients (here, `10.0.3.4` is the IP address of the server):
```
C:\DUKE3D>udpsetup -c 10.0.3.4 -nodes 4 vcommit duke3d
```
See `udpsetup.txt` for more information about this driver.
