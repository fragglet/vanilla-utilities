
# makefile for Borland make

CFLAGS = -O -d -w

LIB = lib\flag.obj lib\log.obj

REPLAY_OBJS = $(LIB) ctrl\replay.obj ctrl\control.obj
ANALOGJS_OBJS = $(LIB) ctrl\analogjs.obj ctrl\control.obj ctrl\joystick.obj
IPXSETUP_OBJS = $(LIB) net\ipxsetup.obj net\doomnet.obj net\ipxnet.obj
SERSETUP_OBJS = $(LIB) net\sersetup.obj net\doomnet.obj net\serport.obj
PARSETUP_OBJS = $(LIB) net\parsetup.obj net\doomnet.obj net\parport.obj \
                net\plio.obj
PASSTHRU_OBJS = $(LIB) net\passthru.obj net\doomnet.obj
SOLO_NET_OBJS = $(LIB) net\solo-net.obj net\doomnet.obj
METANET_OBJS = $(LIB) net\metanet.obj net\doomnet.obj
STATDUMP_OBJS = $(LIB) stat\statdump.obj ctrl\control.obj stat\statprnt.obj \
                stat\stats.obj

EXES = analogjs.exe replay.exe statdump.exe metanet.exe \
       ipxsetup.exe sersetup.exe parsetup.exe solo-net.exe

all: $(EXES)

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
