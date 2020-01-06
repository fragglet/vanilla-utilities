#include <stdio.h>
#include <dos.h>
#include <tcp.h>
#include <elib.h>

static char tempstack[ 4096 ];
static int oldss, oldsp;
static void interrupt (*oldinterrupt)(void);
static void (*userroutine)(void) = NULL;
static int inside = 0;

static void interrupt newinterrupt( void )
{
    (*oldinterrupt)();
    disable();
    if (!sem_up( &inside )) {
        oldss = _SS;
        oldsp = _SP;
        _SP = FP_OFF( &tempstack[ sizeof( tempstack ) - 4 ] );
        _SS = FP_SEG( tempstack );
        enable();
        if (userroutine) (*userroutine)();
        tcp_tick( NULL );
        disable();
        _SP = oldsp;
        _SS = oldss;
        inside = 0;
    }
    enable();
}

void backgroundon( void )
{
    oldinterrupt = getvect( 0x08 );
    setvect( 0x08, newinterrupt );
}

void backgroundoff( void )
{
    setvect( 0x08, oldinterrupt );
}

void backgroundfn( void (*fn)(void) )
{
    userroutine = fn;
}

