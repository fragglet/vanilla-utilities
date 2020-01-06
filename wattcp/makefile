#
#       Makefile for Watstar TCP/IP kernal
#
# switches:
#	define the ones you want in the CFLAGS definition...
#
#	TRACE		- turn on tracing/debugging code
#
#
all:
        if not exist lib\wattcplg.lib md lib
        cd elib
        make
        cd ..\src
        make
        cd ..\apps
        make
        cd ..
