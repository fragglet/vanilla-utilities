`metanet` is a special Doom network driver that builds a packet forwarding
network out of other Doom network drivers. This document explains how to
use it through several example scenarios.  
In the examples, `doom.exe` will be used for the game itself that is being
launched, but this could be any other game or program that uses the Doom
networking API; for example, `heretic.exe` or  `hexen.exe`.

Firstly, some basic principles:

* `metanet` always runs "on top" of other networking drivers. For example,
`sersetup.exe` is used to invoke `metanet.exe` which then invokes
`doom.exe`. It can use more than one underlying driver.

* Every player in the game must use `metanet`.

* A `metanet` network cannot have more than four "hops". That is, if you're
daisy-chaining computers together, there cannot be more than four links in
the chain.

## Example 1: Daisy-chaining three laptops

Almost all '90s-era laptops had at least one serial port and one parallel
port. If you have three such laptops and the appropriate cables, you can
use `metanet` to daisy-chain them together and create a three-player game.
What you'll need:

 * Machine *A*, with at least one serial port
 * Machine *B*, with at least one serial port and one parallel port
 * Machine *C*, with at least one parallel port
 * Null-modem (serial cable)
 * Straight-through parallel port cable
   (aka [laplink cable](https://en.wikipedia.org/wiki/Laplink_cable)).

Connect machine *A* to machine *B* with the null-modem cable, and machine
*B* to machine *C* with the parallel port cable.

Start machine *A* with the following command:
```
sersetup.exe -com1 metanet.exe doom.exe
```
Machine *B* must communicate on two ports (*COM1:* and *LPT1:*), so the
command line looks like this:
```
sersetup.exe -com1 parsetup.exe -lpt1 metanet.exe doom.exe
```
For Machine *C*:
```
parsetup.exe -lpt1 metanet.exe doom.exe
```
This example can be expanded to include a fourth machine, connecting it either
to machine *A* using a second parallel port cable, or to machine *C* using a
second null-modem cable. Note however that this increases the number of hops in
the network, potentially affecting performance (since sending a message from
*A* to *D* requires three hops).

### Example 2: Four player mixed IPX/serial game

In this example, three machines (*A*, *B*, and *C*) have networking cards but a
fourth (*D*) does not. Fortunately *D* can communicate with *C* through a
serial link. `metanet` can be used to establish a four player game.

The command line for *A* and *B* looks like this:
```
ipxsetup.exe -nodes 3 metanet.exe doom.exe
```
Note the `-nodes 3` parameter which may be slightly counterintuitive since
we are establishing a four player game. Here the `-nodes` parameter only
controls the number of machines on the IPX link, of which there are three
(*A*, *B*, *C*).

The command line for *C* looks like this:
```
ipxsetup.exe -nodes 3 sersetup.exe -com1 metanet.exe doom.exe
```
The command line for *D* looks like this:
```
sersetup.exe -com1 metanet.exe doom.exe
```
The null-modem cable connecting *C* and *D* is connected to *COM1:* on both
machines.

### Example 3: Joining two LAN parties

Two LAN parties are taking place in different cities in the same evening,
and the players want to set up an eight player Hexen game over a phone link
(in this example, Hexen is being used because it supports up to eight
players). Four players will be participating at each site, and there will
be a dedicated forwarding machine at each site running the phone link.

This is a fairly complicated example that also introduces `metanet`'s
*forwarding mode*: in forwarding mode, the machine does not participate in
the game itself but merely performs packet forwarding between other
machines. Forwarding mode is useful here to ensure low latency, since
the CPU is not being shared between the game and `metanet`.

Each player starts the game with the following command line:
```
ipxsetup.exe -nodes 5 metanet.exe hexen.exe
```
As with the previous example, `-nodes 5` may be counterintuitive. There are
four players at each site, plus one forwarding machine, which makes for
five nodes at each site in total.

Site A's forwarding machine listens for the phone call from Site B:
```
sersetup.exe -answer -com1 ipxsetup.exe -nodes 5 metanet.exe -forward
```
The ordering here potentially matters: it listens for the incoming phone
call *first*, before joining the IPX game. If it was the other way round,
there's a chance that the incoming call might come while it was still
establishing the IPX link with other local machines.

Side B's forwarding machine initiates the phone call to Site A:
```
sersetup.exe -com1 -dial 555-1212 ipxsetup.exe -nodes 5 metanet.exe -forward
```
Note that neither of the forwarding machines includes `hexen.exe` on the
command line since they do not launch the game. Instead, `-forward` is used
to indicate that `metanet` should simply forward packets once the network
has been constructed.

### Debugging

`metanet` prints some statistics on exit and if a game is not starting up as 
expected, it can be worth checking to see if any of these appear:

* `wrong_magic`: Check that all machines in the network are using `metanet`.
* `too_many_hops`: Only up to four hops are supported in a meta-network.
Try to centralize the network around a smaller number of forwarding
machines.
* `invalid_dest`: Machine is receiving forwarding packets for an unknown
destination. This may indicate packet corruption; check all links are
working okay and consider increasing the baud rate on serial links.
* `unknown_src`: Game packets are being received from an unknown machine on
the meta-network. Check for link corruption, and consider reporting a bug
if no cause can be found.
* `node_limit`: You've hit the limit for the number of nodes in a
meta-network (16).
* `bad_send`: Game is trying to send to an invalid node. If you're using a
source port this may be a bug you should report.
* `unknown_type`: Unknown type of `metanet` packet received. Make sure that
all nodes are using the same `metanet` version.

