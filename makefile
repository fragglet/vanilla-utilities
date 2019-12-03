
# makefile for Borland make

CFLAGS = -O -d

REPLAY_OBJS = ctrl\replay.obj ctrl\control.obj lib\flag.obj
IPXSETUP_OBJS = net\ipxsetup.obj net\doomnet.obj lib\flag.obj net\ipxnet.obj
SERSETUP_OBJS = net\sersetup.obj net\doomnet.obj lib\flag.obj net\serport.obj
PARSETUP_OBJS = net\parsetup.obj net\doomnet.obj lib\flag.obj net\parport.obj \
                net\plio.obj
SOLO_NET_OBJS = net\solo-net.obj net\doomnet.obj lib\flag.obj
METANET_OBJS = net\metanet.obj net\doomnet.obj lib\flag.obj
STATDUMP_OBJS = stat\statdump.obj ctrl\control.obj stat\statprnt.obj \
                lib\flag.obj

EXES = replay.exe statdump.exe \
       ipxsetup.exe sersetup.exe parsetup.exe solo-net.exe

all: $(EXES)

replay.exe: $(REPLAY_OBJS)
	tcc -e$* -o$@ $(REPLAY_OBJS) >nul
ipxsetup.exe: $(IPXSETUP_OBJS)
	tcc -e$* -o$@ $(IPXSETUP_OBJS) >nul
sersetup.exe: $(SERSETUP_OBJS)
	tcc -e$* -o$@ $(SERSETUP_OBJS) >nul
parsetup.exe: $(PARSETUP_OBJS)
	tcc -e$* -o$@ $(PARSETUP_OBJS) >nul
metanet.exe: $(METANET_OBJS)
	tcc -e$* -o$@ $(METANET_OBJS) >nul
statdump.exe: $(STATDUMP_OBJS)
	tcc -e$* -o$@ $(STATDUMP_OBJS) >nul
solo-net.exe: $(SOLO_NET_OBJS)
	tcc -e$* -o$@ $(SOLO_NET_OBJS) >nul

.c.obj:
	tcc $(CFLAGS) -c -o$@ $< >nul
.asm.obj:
	tasm /ml $< $@ >nul

clean:
	del $(EXES)
	del $(REPLAY_OBJS)
	del $(IPXSETUP_OBJS)
	del $(SERSETUP_OBJS)
	del $(PARSETUP_OBJS)
	del $(STATDUMP_OBJS)
