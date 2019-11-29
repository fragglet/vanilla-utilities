tcc -c -oobj\flag     lib\flag.c
tcc -c -oobj\control  ctrl\control.c
tcc -c -oobj\replay   ctrl\replay.c
tcc -c -oobj\doomnet  net\doomnet.c
tcc -c -oobj\ipxnet   net\ipxnet.c
tcc -c -oobj\ipxsetup net\ipxsetup.c
tcc -c -oobj\parport  net\parport.c
tcc -c -oobj\parsetup net\parsetup.c
tasm /ml net\plio.asm obj\plio
tcc -c -oobj\serport  net\serport.c
tcc -c -oobj\sersetup net\sersetup.c
rem tcc -c -oobj\solo-net net\solo-net.c
tcc -c -oobj\statdump stat\statdump.c
tcc -c -oobj\statprnt stat\statprnt.c

tcc -oobj\replay   obj\replay.obj obj\control.obj obj\flag.obj
tcc -oobj\ipxsetup obj\ipxsetup.obj obj\doomnet.obj obj\flag.obj obj\ipxnet.obj
tcc -oobj\sersetup obj\sersetup.obj obj\doomnet.obj obj\flag.obj obj\serport.obj
tcc -oobj\parsetup obj\parsetup.obj obj\doomnet.obj obj\flag.obj obj\parport.obj obj\plio.obj
tcc -oobj\statdump obj\statdump.obj obj\control.obj obj\statprnt.obj obj\flag.obj

