# The Vanilla Utilities

This is the manual for *Vanilla Utilities*, a collection of DOS utilities for
interacting with Doom's external driver APIs.

The Vanilla Utilities are distributed under the GNU General Public License,
version 2. See the file named COPYING.txt for more details. Portions of the
`parsetup` driver are under version 1 of the GNU GPL for historical reasons;
see COPYING1.txt.

## Utilities

These are the utilities that are included in the collection. For more
information on each, check the text file with the same name.

* **udpsetup** - network driver for Internet play over the UDP protocol.
  Multiple network stacks are supported including Winsock1, Winsock2 and the
  DOS MSClient stack. The protocol is the same as DOSbox's IPXNET protocol, so
  it can connect to DOSbox servers and play against emulated machines.
* **ipxsetup** - bugfixed and expanded version of the driver originally shipped
  with Doom.
* **sersetup** - bugfixed and expanded version of the serial/modem driver
  originally included with Doom.
* **parsetup** - parallel port network driver, for play over a Laplink-style
  cable.
* **sirsetup** - driver for running over a half-duplex serial infrared (SIR)
  link (aka IrDA), as commonly found on many late '90s laptops.
* **metanet** - networking driver that combines other networking drivers into a
  packet forwarding network.
* **solo-net** - null/standalone network driver that starts a network game
  without any real connection. Replicates the `-solo-net` parameter found in
  many Doom source ports.
* **analogjs** - PC joystick driver with analog control that is more precise
  than Doom's built in joystick support.
* **replay** - demo replay tool that uses the external control API, so that
  demos can be "continued" by recording a new demo from an old one.
* **statdump** - external statistics driver that can write a text summary.
* **vcommit** - adapter that converts a Doom network driver into a 3D Realms
  *COMMIT* driver, as used for *Duke Nukem 3D*, *Blood*, *Shadow Warrior* and
  various other games.
* **vrottcom** - adapter that converts a Doom network driver into a *Rise of
  the Triad* ROTTCOM driver. This means that all of the above network drivers
  can also be used with ROTT.

## Composability

The original network drivers distributed with DOS Doom were hard-coded to
automatically invoke the game executable. For example, the following might be
used to listen for a phone call and start a deathmatch game:
```
C:\DOOM2>sersetup.exe -answer -deathmatch -warp 1
```
The Vanilla Utilities drivers require the user to type the name of the
executable to run, for example:
```
C:\DOOM2>sersetup.exe -answer doom2.exe -deathmatch -warp 1
```
While more verbose, this has a number of advantages: firstly, a different game
executable can be specified (eg. `mbf.exe`, `heretic.exe` or `hexen.exe`).
Secondly, it allows the drivers to be *composed*: for example, to also hook in
the statdump driver that saves game statistics:
```
C:\DOOM2>sersetup.exe -answer statdump.exe -o stats.txt doom2.exe -deathmatch -warp 1
```
In this example, *sersetup* chains to run *statdump*, which then runs *doom2*.

Or, to use the *vcommit* driver to launch a game of Duke Nukem 3D:
```
C:\DUKE3D>sersetup.exe -answer vcommit.exe duke3d.exe
```

## Using vsetargs

Sometimes, it can be useful for the network drivers to behave like the original
ones, for compatibility reasons. A program named `vsetargs.exe` is included
that can modify any of the Vanilla Utilities to set "baked-in arguments". The
modified program behaves as though you always typed those arguments on the
command line each time you ran it.

For example, to make `sersetup.exe` always invoke doom2.exe:
```
C:\DOOM2>vsetargs sersetup.exe doom2.exe
```
Or, if you wanted it to always launch a deathmatch game:
```
C:\DOOM2>vsetargs sersetup.exe doom2.exe -deathmatch
```
To clear baked-in arguments and restore the original behavior, invoke with a
single `-`:
```
C:\DOOM2>vsetargs sersetup.exe -
```

## Common networking options

All the network drivers support the following options:

* `-extratics <n>`: Sends redundant copies of previous tic data in each game
  packet. This uses more bandwidth but makes the game more resilient to packet
  loss. If you are playing across the Internet, it is a very good idea to use
  this option.

* `-dup <n>`: Reduces the bandwidth requirements of the game by sacrificing
  the resolution of player movement. `-dup 2` halves the number of packets
  sent, `-dup 3` reduces it to a third, and so on. Because networks nowadays
  have massively increased in bandwidth since Doom was originally written, this
  option is largely redundant.

* `-vector <n>`: Manually specifies the interrupt vector number to use for the
  network driver's interrupt service routine. This is usually chosen
  automatically, and there's almost no circumstance in which you should ever
  need to use this.
