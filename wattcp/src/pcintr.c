#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <wattcp.h>

/*
 * pcintr - add interrupt based processing, improve performance
 *          during disk slowdowns
 *
 * wintr_init()     - call once
 * wintr_shutdown() - called automatically
 * wintr_enable()   - enable interrupt based calls
 * wintr_disable()  - diable interrupt based calls (default)
 * (*wintr_chain)() - a place to chain in your own calls, must live
 *                    within something like 1K stack
 *
 */


#define TIMER 0x08
void (*wintr_chain)(void) = NULL;

static byte locstack[ 2048 ];
static word on = 0;
static word inside = 0;
static word oldss, oldsp;
static void interrupt (*oldint)(void);

static void interrupt newint( void )
{
    (*oldint)();
    if ( !sem_up( &inside )) {
        if ( on ) {
            disable();
            oldss = _SS;
            oldsp = _SP;
            _SS = _DS;
            _SP = FP_OFF( &locstack[ sizeof( locstack ) - 4 ]);
            enable();

            if ( wintr_chain )
                (*wintr_chain)();
            tcp_tick( NULL );

            disable();
            _SS = oldss;
            _SP = oldsp;
            enable();
        }
        inside = 0;
    }
}

void wintr_enable( void )
{
    on = 1;
}

void wintr_disable( void )
{
    on = 0;
}

void wintr_shutdown( void )
{
    setvect( TIMER, oldint );
}

void wintr_init( void )
{
    atexit( wintr_shutdown );
    oldint = getvect( TIMER );
    setvect( TIMER, newint );
}

