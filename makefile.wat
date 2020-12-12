
# makefile for OpenWatcom wmake
# To invoke: wmake -f makefile.wat

SOURCE_DIRS = ctrl;lib;net;stat;adapters;test
CFLAGS = -I.

REPLAY_OBJS = ctrl\replay.o ctrl\control.o lib\common.lib
ANALOGJS_OBJS = ctrl\analogjs.o ctrl\control.o ctrl\joystick.o lib\common.lib
IPXSETUP_OBJS = net\ipxsetup.o net\doomnet.o net\ipxnet.o lib\common.lib
SERSETUP_OBJS = net\sersetup.o net\doomnet.o net\serport.o net\serarb.o &
                lib\common.lib
PARSETUP_OBJS = net\parsetup.o net\doomnet.o net\parport.o net\plio.o &
                net\serarb.o lib\common.lib
PASSTHRU_OBJS = net\passthru.o net\doomnet.o lib\common.lib
SOLO_NET_OBJS = net\solo-net.o net\doomnet.o lib\common.lib
METANET_OBJS = net\metanet.o net\doomnet.o lib\common.lib
STATDUMP_OBJS = stat\statdump.o ctrl\control.o stat\statprnt.o stat\stats.o &
                lib\common.lib
VCOMMIT_OBJS = adapters\vcommit.o adapters\fragment.o net\doomnet.o &
               adapters\nodemap.o lib\common.lib
VROTTCOM_OBJS = adapters\vrottcom.o adapters\fragment.o net\doomnet.o &
                adapters\nodemap.o lib\common.lib
FAKEDOOM_OBJS = test\fakedoom.o net\doomnet.o ctrl\control.o &
                stat\stats.o lib\common.lib

EXES = analogjs.exe replay.exe statdump.exe metanet.exe &
       ipxsetup.exe sersetup.exe parsetup.exe solo-net.exe &
       vcommit.exe vrottcom.exe

all: $(EXES)

lib\common.lib: lib\flag.o lib\log.o lib\dos.o lib\ints.o
	del $@
	wlib $@ +lib\flag.o +lib\log.o +lib\dos.o +lib\ints.o
replay.exe: $(REPLAY_OBJS)
	wcl -fe=$@ $(REPLAY_OBJS)
analogjs.exe: $(ANALOGJS_OBJS)
	wcl -fe=$@ $(ANALOGJS_OBJS)
ipxsetup.exe: $(IPXSETUP_OBJS)
	wcl -fe=$@ $(IPXSETUP_OBJS)
sersetup.exe: $(SERSETUP_OBJS)
	wcl -fe=$@ $(SERSETUP_OBJS)
parsetup.exe: $(PARSETUP_OBJS)
	wcl -fe=$@ $(PARSETUP_OBJS)
metanet.exe: $(METANET_OBJS)
	wcl -fe=$@ $(METANET_OBJS)
passthru.exe: $(PASSTHRU_OBJS)
	wcl -fe=$@ $(PASSTHRU_OBJS)
statdump.exe: $(STATDUMP_OBJS)
	wcl -fe=$@ $(STATDUMP_OBJS)
solo-net.exe: $(SOLO_NET_OBJS)
	wcl -fe=$@ $(SOLO_NET_OBJS)
vcommit.exe: $(VCOMMIT_OBJS)
	wcl -fe=$@ $(VCOMMIT_OBJS)
vrottcom.exe: $(VROTTCOM_OBJS)
	wcl -fe=$@ $(VROTTCOM_OBJS)
fakedoom.exe: $(FAKEDOOM_OBJS)
	wcl -fe=$@ $(FAKEDOOM_OBJS)

.EXTENSIONS:
.EXTENSIONS: .exe .o .asm .c

.c: $(SOURCE_DIRS)
.asm: $(SOURCE_DIRS)

.c.o:
	wcc $(CFLAGS) -fo$@ $<
.asm.o:
	wasm -fo=$@ $<

clean:
	del $(EXES)
	del $(ANALOGJS_OBJS)
	del $(IPXSETUP_OBJS)
	del $(METANET_OBJS)
	del $(PARSETUP_OBJS)
	del $(PASSTHRU_OBJS)
	del $(REPLAY_OBJS)
	del $(SERSETUP_OBJS)
	del $(SOLO_NET_OBJS)
	del $(STATDUMP_OBJS)
	del $(VROTTCOM_OBJS)
