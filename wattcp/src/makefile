#
#       Makefile for WatTCP TCP/IP kernal
#
# switches:
#	define the ones you want in the CFLAGS definition...
#
#	TRACE		- turn on tracing/debugging code
#       SKIPINI         - bootp/dhcp only, no WATTCP.CFG (size critical apps)
#       ETH_MSS         - you might set =576 for serial use (EPPPD), mostly
#                          useful if lacking CFG file (SKIPINI, etc)
#       TW_TO           - set between 30 and 240 for an RFC-compliant
#                          TIME_WAIT period (defaults to 2, higher can delay
#                          sock_wait_closed() much longer)
#
DEBUGS= -v

# 'obsolete function' warning can be safely ignored.  There should be
#  no other warnings at the default warning level under bcc3.1 and 4.x;
#  let's keep it that way :)

CFLAGS= $(DEBUGS) -DMSDOS -DETH_MSS=576 -I..\include
MODELS=-ms
MODELL=-ml
MODELH=-mh

# Assembler flags
AFLAGS=-mx -t -i..\elib
AMODELS=-dFUNC_L=0
AMODELL=-dFUNC_L=1

# Define PC-specific include file

# old pre-BC4
#       bcc -c $(MODELS) $(CFLAGS) $*.c > errout

.c.obj:
        tcc -c $(MODELS) $(CFLAGS) $*.c
        tlib ..\lib\wattcpsm -+$*
        tcc -c $(MODELL) $(CFLAGS) $*.c
        tlib ..\lib\wattcplg -+$*
        tcc -c $(MODELH) $(CFLAGS) $*.c
        tlib ..\lib\wattcphg -+$*

.asm.obj:
        tasm $(AFLAGS) $(AMODELS) $*;
        tlib ..\lib\wattcpsm -+$*
        tasm $(AFLAGS) $(AMODELL) $*;
        tlib ..\lib\wattcplg -+$*
        tlib ..\lib\wattcphg -+$*


objs =pcpkt.obj pctcp.obj pcarp.obj pcsed.obj pcicmp.obj pcping.obj asmpkt.obj \
        sock_prn.obj sock_ini.obj pcbsd.obj pcrecv.obj \
        udp_nds.obj udp_dom.obj pcconfig.obj pc_cbrk.obj pcbuf.obj \
        pcbootp.obj sock_dbu.obj pcdbug.obj test.obj pcintr.obj pcstat.obj \
        bsdname.obj fragment.obj \
        netback.obj select.obj

..\lib\wattcpsm.lib: $(objs)
        echo done

fragment.obj: fragment.c ..\include\wattcp.h
select.obj: select.c ..\include\wattcp.h
pcicmp.obj: pcicmp.c ..\include\wattcp.h
pcpkt.obj: pcpkt.c ..\include\wattcp.h
pctcp.obj: pctcp.c ..\include\wattcp.h
pcarp.obj: pcarp.c ..\include\wattcp.h
wattcpd.obj: wattcpd.c ..\include\wattcp.h
pcping.obj: pcping.c ..\include\wattcp.h
pcsed.obj: pcsed.c ..\include\wattcp.h ..\include\ethdev.h
sock_prn.obj: sock_prn.c ..\include\wattcp.h
sock_ini.obj: sock_ini.c ..\include\wattcp.h
udp_nds.obj: udp_nds.c ..\include\wattcp.h
udp_dom.obj: udp_dom.c ..\include\wattcp.h
pcconfig.obj: pcconfig.c ..\include\wattcp.h
pc_cbrk.obj: pc_cbrk.c ..\include\wattcp.h
sock_dbu.obj: sock_dbu.c ..\include\wattcp.h
pcbsd.obj: pcbsd.c ..\include\tcp.h ..\include\wattcp.h
pcbuf.obj: pcbuf.c ..\include\wattcp.h
pcbootp.obj: pcbootp.c ..\include\wattcp.h ..\include\bootp.h
pcrecv.obj: pcrecv.c ..\include\wattcp.h
asmpkt.obj: asmpkt.asm
pcdbug.obj: pcdbug.c ..\include\wattcp.h
test.obj : test.c ..\include\wattcp.h
pcintr.obj: pcintr.c ..\include\wattcp.h ..\include\elib.h
pcstat.obj: pcstat.c ..\include\wattcp.h
netback.obj: netback.c ..\include\elib.h ..\include\tcp.h
bsdname.obj: bsdname.c ..\include\wattcp.h
