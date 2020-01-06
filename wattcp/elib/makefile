#
# ELIB - library of MSC/TC compatible functions
#        and old MASM/new MASM/TASM compatible too
#

# This makes libraries of small, large, and huge model functions,
#
CFLAGS= -w -a -d -f- -G -O -Z -DMSDOS -I..\include
MODELS=-ms
MODELL=-ml
MODELH=-mh

# Assembler flags
AFLAGS=-mx -t
AMODELS=-dFUNC_L=0
AMODELL=-dFUNC_L=1

# Define PC-specific include file

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

start: elib.lib

ELIB.LIB: OUTHEXES.OBJ OUTCH.OBJ \
OUTHEX.OBJ OUTS.OBJ INTEL.OBJ QMOVE.OBJ \
SEMAPHOR.OBJ TIMEOUT.OBJ

