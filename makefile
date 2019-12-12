
# makefile for Borland make

CFLAGS = -O -d -w

REPLAY_OBJS = ctrl\replay.obj ctrl\control.obj lib\flag.obj
IPXSETUP_OBJS = net\ipxsetup.obj net\doomnet.obj lib\flag.obj net\ipxnet.obj \
                lib\log.obj
SERSETUP_OBJS = net\sersetup.obj net\doomnet.obj lib\flag.obj \
                net\serport.obj lib\log.obj
PARSETUP_OBJS = net\parsetup.obj net\doomnet.obj lib\flag.obj net\parport.obj \
                net\plio.obj lib\log.obj
PASSTHRU_OBJS = net\passthru.obj net\doomnet.obj lib\flag.obj
SOLO_NET_OBJS = net\solo-net.obj net\doomnet.obj lib\flag.obj
METANET_OBJS = net\metanet.obj net\doomnet.obj lib\flag.obj
STATDUMP_OBJS = stat\statdump.obj ctrl\control.obj stat\statprnt.obj \
                lib\flag.obj stat\stats.obj

EXES = replay.exe statdump.exe metanet.exe \
       ipxsetup.exe sersetup.exe parsetup.exe solo-net.exe

all: $(EXES)

replay.exe: $(REPLAY_OBJS)
	tcc -e$* -o$@ $(REPLAY_OBJS)
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
