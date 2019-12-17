
This is a collection of DOS utilities for interacting with Doom's external
driver APIs. Specifically it includes improved versions of Doom's network
drivers (IPXSETUP and SERSETUP) along with APIs for the
[network](https://doomwiki.org/wiki/Doom_networking_component#External_drivers),
[control](https://doomwiki.org/wiki/External_control_driver), and
[statistics](https://doomwiki.org/wiki/Statistics_driver) interfaces.

Design principles here are:

* **Composability** - it is possible to combine multiple tools and use them
together.
* **Reusability** - the codebase has clearly-defined APIs for interacting
with Doom's command line interfaces, so that making new tools is
straightforward.

## Utilities

* **ipxsetup** and **sersetup** - bugfixed and expanded versions of the IPX
and serial/modem drivers originally included with Doom.
* **parsetup** - parallel port network driver, derived from
[the version from the idgames archive](https://www.doomworld.com/idgames/utils/serial/psetup11).
* **metanet** - networking driver that combines other networking drivers
into a packet forwarding network. This allows, for example, three players on
an IPX network to be joined by a fourth on a serial cable.
* **solo-net** - null/standalone network driver that starts a network game
without any real connection. Replicates the `-solo-net` parameter found in
many Doom source ports.
* **analogjs** - PC joystick driver with analog control that is more precise
than Doom's built in joystick support.
* **replay** - demo replay tool that uses the external control API, so that
demos can be "continued" by recording a new demo from an old one.
* **statdump** - external statistics driver that can write a text summary.

