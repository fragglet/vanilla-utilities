
# makefile for OpenWatcom wmake
# To invoke: wmake -f makefile.wat

SOURCE_DIRS = ctrl;lib;net;stat;adapters;test
CFLAGS = -I. -q
LDFLAGS = -q

REPLAY_OBJS = ctrl\replay.o ctrl\control.o lib\common.lib
ANALOGJS_OBJS = ctrl\analogjs.o ctrl\control.o ctrl\joystick.o lib\common.lib
IPXSETUP_OBJS = net\ipxsetup.o net\doomnet.o net\ipxnet.o net\llcall.o &
                lib\common.lib
SERSETUP_OBJS = net\sersetup.o net\doomnet.o net\serport.o net\serarb.o &
                lib\common.lib
SIRSETUP_OBJS = net\sirsetup.o net\doomnet.o net\serport.o net\serarb.o &
                lib\common.lib
PARSETUP_OBJS = net\parsetup.o net\doomnet.o net\parport.o net\plio.o &
                net\serarb.o lib\common.lib
UDPSETUP_OBJS = net\udpsetup.o net\doomnet.o net\dossock.o net\llcall.o &
                lib\common.lib
PASSTHRU_OBJS = net\passthru.o net\doomnet.o lib\common.lib
SOLO_NET_OBJS = net\solo-net.o net\doomnet.o lib\common.lib
METANET_OBJS = net\metanet.o net\doomnet.o lib\common.lib
STATDUMP_OBJS = stat\statdump.o ctrl\control.o stat\statprnt.o stat\stats.o &
                lib\common.lib
VCOMMIT_OBJS = adapters\vcommit.o adapters\fragment.o net\doomnet.o &
               adapters\nodemap.o lib\common.lib
VROTTCOM_OBJS = adapters\vrottcom.o adapters\fragment.o net\doomnet.o &
                adapters\nodemap.o lib\common.lib
VSETARGS_OBJS = lib\vsetargs.o
FAKEDOOM_OBJS = test\fakedoom.o net\doomnet.o ctrl\control.o &
                stat\stats.o lib\common.lib

EXES = analogjs.exe replay.exe statdump.exe metanet.exe &
       ipxsetup.exe sersetup.exe parsetup.exe solo-net.exe &
       vcommit.exe vrottcom.exe vsetargs.exe sirsetup.exe &
       udpsetup.exe

all: $(EXES)
tests: fakedoom.exe

lib\common.lib: lib\flag.o lib\log.o lib\dos.o lib\ints.o
	del $@
	wlib -q $@ +lib\flag.o +lib\log.o +lib\dos.o +lib\ints.o
replay.exe: $(REPLAY_OBJS)
	wcl -q -fe=$@ $(REPLAY_OBJS)
analogjs.exe: $(ANALOGJS_OBJS)
	wcl -q -fe=$@ $(ANALOGJS_OBJS)
ipxsetup.exe: $(IPXSETUP_OBJS)
	wcl -q -fe=$@ $(IPXSETUP_OBJS)
sersetup.exe: $(SERSETUP_OBJS)
	wcl -q -fe=$@ $(SERSETUP_OBJS)
sirsetup.exe: $(SIRSETUP_OBJS)
	wcl -q -fe=$@ $(SIRSETUP_OBJS)
parsetup.exe: $(PARSETUP_OBJS)
	wcl -q -fe=$@ $(PARSETUP_OBJS)
udpsetup.exe: $(UDPSETUP_OBJS)
	wcl -q -fe=$@ $(UDPSETUP_OBJS)
metanet.exe: $(METANET_OBJS)
	wcl -q -fe=$@ $(METANET_OBJS)
passthru.exe: $(PASSTHRU_OBJS)
	wcl -q -fe=$@ $(PASSTHRU_OBJS)
statdump.exe: $(STATDUMP_OBJS)
	wcl -q -fe=$@ $(STATDUMP_OBJS)
solo-net.exe: $(SOLO_NET_OBJS)
	wcl -q -fe=$@ $(SOLO_NET_OBJS)
vcommit.exe: $(VCOMMIT_OBJS)
	wcl -q -fe=$@ $(VCOMMIT_OBJS)
vrottcom.exe: $(VROTTCOM_OBJS)
	wcl -q -fe=$@ $(VROTTCOM_OBJS)
vsetargs.exe: $(VSETARGS_OBJS)
	wcl -q -fe=$@ $(VSETARGS_OBJS)
fakedoom.exe: $(FAKEDOOM_OBJS)
	wcl -q -fe=$@ $(FAKEDOOM_OBJS)

# TODO: Not yet included in EXES until we have a working UDP/IP version
# of IPXSETUP.
ws2patch.exe: net\ws2patch.c
	wcl386 $(CFLAGS) -fe=$@ $< -l=dos32a

.EXTENSIONS:
.EXTENSIONS: .exe .o .asm .c

.c: $(SOURCE_DIRS)
.asm: $(SOURCE_DIRS)

.c.o:
	wcc $(CFLAGS) -fo$@ $<
.asm.o:
	wasm -q -fo=$@ $<

clean:
	del $(EXES)
	del $(ANALOGJS_OBJS)
	del $(IPXSETUP_OBJS)
	del $(METANET_OBJS)
	del $(PARSETUP_OBJS)
	del $(PASSTHRU_OBJS)
	del $(REPLAY_OBJS)
	del $(SERSETUP_OBJS)
	del $(SIRSETUP_OBJS)
	del $(SOLO_NET_OBJS)
	del $(STATDUMP_OBJS)
	del $(VROTTCOM_OBJS)
