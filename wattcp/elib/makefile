#
# ELIB - library of MSC/TC compatible functions
#        and old MASM/new MASM/TASM compatible too
#

# This makes libraries of small, large, and huge model functions,
#
CFLAGS= -w -a -d -f- -G -O -Z -DMSDOS -I..\include -IC:\borlandc\include
#CFLAGS= -w -a -d -f- -G -O -Z -DMSDOS -I..\include -IC:\bc4\include
MODELS=-ms
MODELL=-ml
MODELH=-mh

# Assembler flags
AFLAGS=-mx -t
AMODELS=-dFUNC_L=0
AMODELL=-dFUNC_L=1

# Define PC-specific include file

.c.obj:
        bcc -c $(MODELS) $(CFLAGS) $*.c
        tlib ..\lib\wattcpsm -+$*
        bcc -c $(MODELL) $(CFLAGS) $*.c
        tlib ..\lib\wattcplg -+$*
        bcc -c $(MODELH) $(CFLAGS) $*.c
        tlib ..\lib\wattcphg -+$*

.asm.obj:
        tasm $(AFLAGS) $(AMODELS) $*;
        tlib ..\lib\wattcpsm -+$*
        tasm $(AFLAGS) $(AMODELL) $*;
        tlib ..\lib\wattcplg -+$*
        tlib ..\lib\wattcphg -+$*

start: elib.lib

ELIB.LIB: QMSG.OBJ OUTHEXES.OBJ BACKGRND.OBJ DOSLIST.OBJ KEEP.OBJ OUTCH.OBJ \
OUTHEX.OBJ OUTS.OBJ OUTSN.OBJ INTEL.OBJ INCHKSUM.OBJ QMOVE.OBJ QCMP.OBJ \
SEMAPHOR.OBJ TIMEOUT.OBJ STRINGS.OBJ ISSTRING.OBJ

