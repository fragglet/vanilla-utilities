
# makefile for OpenWatcom wmake
# To invoke: wmake -f makefile.wat

SOURCE_DIRS = ctrl;lib;net;stat;adapters;test
CFLAGS = -I. -q -onx -w3
LDFLAGS = -q

REPLAY_OBJS = bld\replay.o bld\control.o bld\common.lib
ANALOGJS_OBJS = bld\analogjs.o bld\control.o bld\joystick.o bld\common.lib
IPXSETUP_OBJS = bld\ipxsetup.o bld\doomnet.o bld\ipxnet.o bld\llcall.o &
                bld\common.lib
SERSETUP_OBJS = bld\sersetup.o bld\doomnet.o bld\serport.o bld\serarb.o &
                bld\common.lib
SIRSETUP_OBJS = bld\sirsetup.o bld\doomnet.o bld\serport.o bld\serarb.o &
                bld\common.lib
PARSETUP_OBJS = bld\parsetup.o bld\doomnet.o bld\parport.o bld\plio.o &
                bld\serarb.o bld\common.lib
UDPSETUP_OBJS = bld\ipxsetup.o bld\doomnet.o bld\udpipx.o bld\llcall.o &
                bld\common.lib bld\dossock.o bld\dbserver.o
PASSTHRU_OBJS = bld\passthru.o bld\doomnet.o bld\common.lib
SOLO_NET_OBJS = bld\solo-net.o bld\doomnet.o bld\common.lib
METANET_OBJS = bld\metanet.o bld\doomnet.o bld\common.lib bld\pktaggr.o
STATDUMP_OBJS = bld\statdump.o bld\control.o bld\statprnt.o bld\stats.o &
                bld\common.lib
VCOMMIT_OBJS = bld\vcommit.o bld\fragment.o bld\doomnet.o &
               bld\nodemap.o bld\common.lib
VROTTCOM_OBJS = bld\vrottcom.o bld\fragment.o bld\doomnet.o &
                bld\nodemap.o bld\common.lib
VSETARGS_OBJS = bld\vsetargs.o
FAKEDOOM_OBJS = bld\fakedoom.o bld\doomnet.o bld\control.o &
                bld\stats.o bld\common.lib

EXES = bld\analogjs.exe bld\replay.exe bld\statdump.exe bld\metanet.exe &
       bld\ipxsetup.exe bld\sersetup.exe bld\parsetup.exe bld\solo-net.exe &
       bld\vcommit.exe bld\vrottcom.exe bld\vsetargs.exe bld\sirsetup.exe &
       bld\udpsetup.exe bld\ws2patch.exe

all: exes tests
exes: $(EXES)
tests: test\fakedoom.exe

bld\common.lib: bld\flag.o bld\log.o bld\dos.o bld\ints.o
	wlib -q -n $@ +bld\flag.o +bld\log.o +bld\dos.o +bld\ints.o
bld\replay.exe: $(REPLAY_OBJS)
	wcl -q -fe=$@ $(REPLAY_OBJS)
bld\analogjs.exe: $(ANALOGJS_OBJS)
	wcl -q -fe=$@ $(ANALOGJS_OBJS)
bld\ipxsetup.exe: $(IPXSETUP_OBJS)
	wcl -q -fe=$@ $(IPXSETUP_OBJS)
bld\sersetup.exe: $(SERSETUP_OBJS)
	wcl -q -fe=$@ $(SERSETUP_OBJS)
bld\sirsetup.exe: $(SIRSETUP_OBJS)
	wcl -q -fe=$@ $(SIRSETUP_OBJS)
bld\parsetup.exe: $(PARSETUP_OBJS)
	wcl -q -fe=$@ $(PARSETUP_OBJS)
bld\udpsetup.exe: $(UDPSETUP_OBJS)
	wcl -q -fe=$@ $(UDPSETUP_OBJS)
bld\metanet.exe: $(METANET_OBJS)
	wcl -q -fe=$@ $(METANET_OBJS)
bld\passthru.exe: $(PASSTHRU_OBJS)
	wcl -q -fe=$@ $(PASSTHRU_OBJS)
bld\statdump.exe: $(STATDUMP_OBJS)
	wcl -q -fe=$@ $(STATDUMP_OBJS)
bld\solo-net.exe: $(SOLO_NET_OBJS)
	wcl -q -fe=$@ $(SOLO_NET_OBJS)
bld\vcommit.exe: $(VCOMMIT_OBJS)
	wcl -q -fe=$@ $(VCOMMIT_OBJS)
bld\vrottcom.exe: $(VROTTCOM_OBJS)
	wcl -q -fe=$@ $(VROTTCOM_OBJS)
bld\vsetargs.exe: $(VSETARGS_OBJS)
	wcl -q -fe=$@ $(VSETARGS_OBJS)
test\fakedoom.exe: $(FAKEDOOM_OBJS)
	wcl -q -fe=$@ $(FAKEDOOM_OBJS)

# TODO: Not yet included in EXES until we have a working UDP/IP version
# of IPXSETUP.
bld\ws2patch.exe: net\ws2patch.c
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
	del bld\*.o
	del bld\*.exe
	del bld\*.lib
	del test\fakedoom.exe
