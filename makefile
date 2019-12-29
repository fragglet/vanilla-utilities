
# makefile for Borland make

CFLAGS = -O -d -w

REPLAY_OBJS = ctrl\replay.obj ctrl\control.obj lib\common.lib
ANALOGJS_OBJS = ctrl\analogjs.obj ctrl\control.obj ctrl\joystick.obj \
                lib\common.lib
IPXSETUP_OBJS = net\ipxsetup.obj net\doomnet.obj net\ipxnet.obj lib\common.lib
SERSETUP_OBJS = net\sersetup.obj net\doomnet.obj net\serport.obj lib\common.lib
PARSETUP_OBJS = net\parsetup.obj net\doomnet.obj net\parport.obj \
                net\plio.obj lib\common.lib
PASSTHRU_OBJS = net\passthru.obj net\doomnet.obj lib\common.lib
SOLO_NET_OBJS = net\solo-net.obj net\doomnet.obj lib\common.lib
METANET_OBJS = net\metanet.obj net\doomnet.obj lib\common.lib
STATDUMP_OBJS = stat\statdump.obj ctrl\control.obj stat\statprnt.obj \
                stat\stats.obj lib\common.lib

EXES = analogjs.exe replay.exe statdump.exe metanet.exe \
       ipxsetup.exe sersetup.exe parsetup.exe solo-net.exe

all: $(EXES)

lib\common.lib: lib\flag.obj lib\log.obj
	tlib $@ +lib\flag.obj +lib\log.obj
replay.exe: $(REPLAY_OBJS)
	tcc -e$* -o$@ $(REPLAY_OBJS)
analogjs.exe: $(ANALOGJS_OBJS)
	tcc -e$* -o$@ $(ANALOGJS_OBJS)
ipxsetup.exe: $(IPXSETUP_OBJS)
	tcc -e$* -o$@ $(IPXSETUP_OBJS)
sersetup.exe: $(SERSETUP_OBJS)
	tcc -e$* -o$@ $(SERSETUP_OBJS)
parsetup.exe: $(PARSETUP_OBJS)
	tcc -e$* -o$@ $(PARSETUP_OBJS)
metanet.exe: $(METANET_OBJS)
	tcc -e$* -o$@ $(METANET_OBJS)
passthru.exe: $(PASSTHRU_OBJS)
	tcc -e$* -o$@ $(PASSTHRU_OBJS)
statdump.exe: $(STATDUMP_OBJS)
	tcc -e$* -o$@ $(STATDUMP_OBJS)
solo-net.exe: $(SOLO_NET_OBJS)
	tcc -e$* -o$@ $(SOLO_NET_OBJS)

.c.obj:
	tcc $(CFLAGS) -c -o$@ $<
.asm.obj:
	tasm /ml $< $@

clean:
	del $(EXES)
	del $(REPLAY_OBJS)
	del $(IPXSETUP_OBJS)
	del $(SERSETUP_OBJS)
	del $(PARSETUP_OBJS)
	del $(STATDUMP_OBJS)
