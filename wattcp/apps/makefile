#
#       Makefile for Waterloo TCP sample applications
#

TCCINCLUDE=C:\borlandc\include
DEBUG= E        # set to D for disable, E for enable
MODEL= L        # set to L for large, S for small

#
#
# auto configure section
#
#

!if '$(DEBUG)'=='E'
IDEBUG=-v
TEXTDEBUG=enabled
!elif '$(DEBUG)'=='D'
IDEBUG=-v-
TEXTDEBUG=disabled
#!else
#!error  DEBUG must be set to either E or D
!endif

!if '$(MODEL)'=='L'
CMODEL=-ml
CLIB=..\lib\wattcplg.lib
TEXTMODEL=large
!elif '$(MODEL)'=='S'
CMODEL=-ms
CLIB=..\lib\wattcpsm.lib
TEXTMODEL=small
!else
!error  MODEL must be set to either S or L
!endif

CFLAGS= $(CMODEL) -r- $(IDEBUG) -IC:\borlandc\include -I..\include
#CFLAGS= $(CMODEL) -r- $(IDEBUG) -IC:\bc4\include -I..\include
CC= bcc $(CFLAGS)

#
#
#  list of executables
#
#

.c.exe:
        $(CC) $*.c $(CLIB)


exes.arc: ping.exe daytime.exe finger.exe lpr.exe lpq.exe \
        tcpport.exe rexec.exe ntime.exe tcpinfo.exe cookie.exe \
        popdump.exe ph.exe tcptalk.exe ftp.exe
# host.exe
        echo done $(TEXTMODEL) make with debugging $(TEXTDEBUG)

ftp.exe: ftp.c

cookie.exe: cookie.c

popdump.exe: popdump.c

ping.exe: ping.c
        $(CC)  ping.c $(CLIB)

daytime.exe: daytime.c
        $(CC) daytime.c $(CLIB)

finger.exe: finger.c

lpr.exe: lpr.c
        $(CC)  lpr.c $(CLIB)

lpq.exe: lpq.c
        $(CC)  lpq.c $(CLIB)

tcpport.exe: tcpport.c
        $(CC)  tcpport.c $(CLIB)

# test version of tcpport
tcpport1.exe: tcpport1.c
        $(CC)  tcpport1.c  $(CLIB)

rexec.exe: rexec.c
        $(CC)  rexec.c $(CLIB)

ntime.exe: ntime.c
        $(CC)  ntime.c $(CLIB)

tcpinfo.exe: tcpinfo.c
        $(CC)  tcpinfo.c $(CLIB)

ph.exe: ph.c
        $(CC)  ph.c  $(CLIB)

tcptalk.exe: tcptalk.c
        $(CC)  tcptalk.c $(CLIB)

host.exe: host.c
        $(CC)  host.c $(CLIB)
