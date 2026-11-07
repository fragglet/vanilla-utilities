# vcommit

This is the manual for the `vcommit` driver included with Vanilla Utilities.

The `vcommit` driver allows Doom network drivers to be used with games from 3D
Realms that support the COMMIT network interface. These include *Duke Nukem 3D*,
*Blood*, *Shadow Warrior*, *Redneck Rampage* and others.

## Command line arguments

The driver has no command line options.

## Examples

The `vcommit` driver has no special command line options of its own. It must be
invoke it from a Doom network driver, and it then invokes the game.

The following simple example uses the `parsetup` driver to play Duke Nukem 3D
over a parallel port connection (Laplink cable):
```
C:\DUKE3D>parsetup.exe vcommit.exe duke3d.exe
```
