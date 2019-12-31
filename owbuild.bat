wcc -I. -fo=ctrl\analogjs.o  ctrl\analogjs.c
wcc -I. -fo=ctrl\control.o  ctrl\control.c
wcc -I. -fo=ctrl\replay.o  ctrl\replay.c
wasm -fo=ctrl\joystick.o ctrl\joystick.asm
wcc -I. -fo=lib\dos.o  lib\dos.c
wcc -I. -fo=lib\flag.o  lib\flag.c
wcc -I. -fo=lib\log.o  lib\log.c
wcc -I. -fo=net\doomnet.o  net\doomnet.c
wcc -I. -fo=net\ipxnet.o  net\ipxnet.c
wcc -I. -fo=net\ipxsetup.o  net\ipxsetup.c
wcc -I. -fo=net\metanet.o  net\metanet.c
wcc -I. -fo=net\parport.o  net\parport.c
wcc -I. -fo=net\parsetup.o  net\parsetup.c
wasm -fo=net\plio.o  net\plio.asm
wcc -I. -fo=net\passthru.o  net\passthru.c
wcc -I. -fo=net\serport.o  net\serport.c
wcc -I. -fo=net\sersetup.o  net\sersetup.c
wcc -I. -fo=net\solo-net.o  net\solo-net.c
wcc -I. -fo=stat\statdump.o  stat\statdump.c
wcc -I. -fo=stat\statprnt.o  stat\statprnt.c
wcc -I. -fo=stat\stats.o  stat\stats.c

wcl ctrl\replay.o ctrl\control.o lib\flag.o lib\dos.o lib\log.o
wcl ctrl\analogjs.o ctrl\control.o ctrl\joystick.o lib\flag.o lib\dos.o lib\log.o
wcl net\ipxsetup.o net\doomnet.o net\ipxnet.o lib\flag.o lib\dos.o lib\log.o
wcl net\sersetup.o net\doomnet.o net\serport.o lib\flag.o lib\dos.o lib\log.o
wcl net\parsetup.o net\doomnet.o net\parport.o net\plio.o lib\flag.o lib\dos.o lib\log.o
wcl net\passthru.o net\doomnet.o lib\flag.o lib\dos.o lib\log.o
wcl net\solo-net.o net\doomnet.o lib\flag.o lib\dos.o lib\log.o
wcl net\metanet.o net\doomnet.o lib\flag.o lib\dos.o lib\log.o
wcl stat\statdump.o ctrl\control.o stat\statprnt.o stat\stats.o lib\flag.o lib\dos.o lib\log.o

