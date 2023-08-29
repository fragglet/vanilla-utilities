`udpsetup` is a Doom network driver that communicates over the UDP/IP
protocol, allowing Doom multiplayer games to be played over the
Internet using the original DOS versions.

The driver supports multiple TCP/IP networking stacks, but crucially it
supports the Winsock driver that exists in Windows 9x (to do this it
makes use of undocumented operating system backdoors that allow 16-bit
DOS programs to access the Internet stack). It also supports the
MSClient TCP/IP stack that Microsoft shipped as part of its "MS Client
3.0", MS LAN Manager, and "Workgroup Add-On for MS-DOS" products,
allowing the driver to run under pure DOS.

A demo video [can be found here](https://youtu.be/1PLXPSP7ZBE).

The following sections give some stack-specific information:

### Winsock 1

This is the original version of the Windows networking stack that
shipped with Windows 95 and Windows for Workgroups 3.1. This should just
work out of the box.

### Winsock 2

This second version of Winsock shipped with Windows 98 and later, but
there was also an upgrade patch that allowed it to be used on Windows
95. Winsock2 has a driver bug that has to be worked around to be used;
after each boot the `WS2PATCH.EXE` program must be run before `udpsetup`
can be used.

### MSClient

The MSClient stack shipped as part of Microsoft's "MS Client 3.0", MS
LAN Manager, and "Workgroup Add-On for MS-DOS" products. It runs under
pure DOS and was used to allow DOS users to integrate into Windows
networks.

The MSClient stack requires some initial work to set up, including
providing appropriate drivers if your network card - real or emulated -
is not one of those supported by the stack out of the box.  The stack
uses quite a lot of memory so you will probably want to tweak your
system so that you only load the stack when you want to use it.

Once set up, `udpsetup` should work out of the box; the only important
detail is that you need to run `SOCKETS.EXE` to load the sockets driver
first.

## Setting up a game

`udpsetup` is client-server based; one player acts as the server and all
data from other players is routed through it. If playing over the
Internet this may mean you need to set up a port forward so that the
other players can connect to the server. How to do this depends on your
particular brand of router and is out of scope for this document; it's
suggested that you search the web for information.

### Starting the server

The server is started using the `-s` option. For example, to set up a
four player deathmatch game:
```
C:\DOOM>udpsetup -s -nodes 4 doom -deathmatch
```
By default the server runs on UDP port 213. To change this, use the `-udpport`
command line option. For example to listen on port 10000 instead:
```
C:\DOOM>udpsetup -s -nodes 4 -udpport 10000 doom -deathmatch
```

### Connecting to the server

Other players connect to the server using the `-c` option (it is not necessary
for clients to set up a port forward). For example:
```
C:\DOOM>udpsetup -c 10.0.3.4 -nodes 4 doom -deathmatch
```
Unfortunately at the time of writing, DNS names are not supported (you
*must* provide an IP address).

As with other DOS Doom network drivers, the same command line options
must be provided by all players or a desync may occur.
Note that it is also necessary to provide the name of the `.exe` program
to run (when playing Doom, usually `DOOM.EXE` or `DOOM2.EXE`). The
reason for this is covered in the next section.

## Playing other games

### Source ports

`udpsetup` can also be used with DOS source ports that support the original
network driver interface. For example, to use the `MBF` (Marine's Best Friend)
port:
```
C:\DOOM>udpsetup -s -nodes 4 mbf -deathmatch
```
and
```
C:\DOOM>udpsetup -c 10.0.3.4 -nodes 4 mbf -deathmatch
```

### Heretic, Hexen, Strife, Chex Quest and others

`udpsetup` can be run with other Doom engine-based games -- all the
commercially released Doom engine games also supported multiplayer. For
example, to set up a Hexen game:
```
C:\HEXEN>udpsetup -s -nodes 4 hexen -deathmatch
```
and
```
C:\HEXEN>udpsetup -c 10.0.3.4 -nodes 4 hexen -deathmatch
```

### Duke Nukem 3D and other BUILD engine games

`udpsetup` can be used with BUILD engine games by using the `vcommit` adapter
driver that also ships with the Vanilla Utilities.
A demo video [can be found here](https://youtu.be/4L5wVLp5wVE).
For example, to start a Duke Nukem 3D game:
```
C:\DUKE3D>udpsetup -s -nodes 4 vcommit duke3d
```
and
```
C:\DUKE3D>udpsetup -c 10.0.3.4 -nodes 4 vcommit duke3d
```

### Rise of the Triad

`udpsetup` can be used to play *Rise of the Triad* using the `vrottcom` adapter
that ships with the Vanilla Utilities. For example:
```
C:\ROTT>udpsetup -s -nodes 4 vrottcom rott
```
and
```
C:\ROTT>udpsetup -c 10.0.3.4 -nodes 4 vrottcom rott
```

## Interfacing with DOSbox

Beneath the hood, `udpsetup` uses the same protocol as DOSbox uses for IPX
tunnelling. Essentially a "virtual" `ipxsetup` game is being run, that DOSbox
clients can communicate with. This allows emulated Doom clients running inside
DOSbox to interact with real vintage DOS/Windows 9x machines.

### Connecting to a DOSbox server

Connecting to a DOSbox server is the same as connecting to any other
server. To start a DOSbox server in the usual way on UDP port 10000:
```
C:\DOOM>config -set ipx true
C:\DOOM>ipxnet startserver 10000
C:\DOOM>ipxsetup -nodes 3 doom -deathmatch
```

Then to connect to the server using `udpsetup` if it were running at
`34.66.233.240`:
```
C:\DOOM>udpsetup -c 34.66.233.240:10000 -nodes 3 doom -deathmatch
```

DOSbox clients connect to the server in the usual way and also use `ipxsetup`:
```
C:\DOOM>config -set ipx true
C:\DOOM>ipxnet connect 34.66.233.240 10000
C:\DOOM>ipxsetup -nodes 3 doom -deathmatch
```

### Connecting to a udpsetup server from DOSbox

DOSbox can connect to a `udpsetup` server the same as any other server. For
example to start a server on port 10000:
```
C:\DOOM>udpsetup -s -udpport 10000 -nodes 3 doom -deathmatch
```

DOSbox clients can then connect to the server and use `ipxsetup` (in this
example the server is running at `34.66.233.240`):
```
C:\DOOM>config -set ipx true
C:\DOOM>ipxnet connect 34.66.233.240 10000
C:\DOOM>ipxsetup -nodes 3 doom -deathmatch
```

Other udpsetup` clients can also connect:
```
C:\DOOM>udpsetup -c 34.66.233.240:10000 -nodes 3 doom -deathmatch
```

### Chocolate Doom

[Chocolate Doom](https://www.chocolate-doom.org/) also supports the DOSbox
protocol, at least on the `vanilla-net` development branch. To connect
Chocolate Doom to a `udpsetup` server:
```
C:\DOOM>udpsetup -s -udpport 10000 doom -deathmatch
```
and
```shell
$ chocolate-doom -dbconnect 10.0.3.4:10000 -deathmatch -iwad doom.wad
```

At the time of writing Chocolate Doom does not have a built-in server.
