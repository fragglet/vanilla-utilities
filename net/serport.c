// serport.c

#include "lib/flag.h"
#include "net/doomnet.h"
#include "net/sersetup.h"

void JumpStart(void);

void interrupt ISR8250(void);

static union REGS regs;
static struct SREGS sregs;

que_t inque, outque;

int uart;                       // io address
static enum { UART_8250, UART_16550 } uart_type;
int irq;

static int modem_status = -1;
static int line_status = -1;

static void interrupt(*oldirqvect) (void);
static int irqintnum;

static int comport;

// Flags:
static int com2 = 0, com3 = 0, com4 = 0;
static int port_flag = 0, irq_flag = 0;

void SerialRegisterFlags(void)
{
    BoolFlag("-com2", &com2, "(and -com3, -com4) use COMx instead of COM1");
    BoolFlag("-com3", &com3, NULL);
    BoolFlag("-com4", &com4, NULL);
    IntFlag("-port", &port_flag, "port", "explicit I/O port for UART");
    IntFlag("-irq", &irq_flag, "irq", "explicit IRQ number for UART");
}

/*
==============
=
= GetUart
=
==============
*/

void GetUart(void)
{
    char far *system_data;
    static int ISA_uarts[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
    static int ISA_IRQs[] = { 4, 3, 4, 3 };
    static int MCA_uarts[] = { 0x03f8, 0x02f8, 0x3220, 0x3228 };
    static int MCA_IRQs[] = { 4, 3, 3, 3 };
    int p;

    if (com2)
        comport = 2;
    else if (com3)
        comport = 3;
    else if (com4)
        comport = 4;
    else
        comport = 1;

    regs.h.ah = 0xc0;
    int86x(0x15, &regs, &regs, &sregs);
    if (regs.x.cflag)
    {
        irq = ISA_IRQs[comport - 1];
        uart = ISA_uarts[comport - 1];
        return;
    }
    system_data = (char far *)(((long)sregs.es << 16) + regs.x.bx);
    if (system_data[5] & 0x02)
    {
        irq = MCA_IRQs[comport - 1];
        uart = MCA_uarts[comport - 1];
    }
    else
    {
        irq = ISA_IRQs[comport - 1];
        uart = ISA_uarts[comport - 1];
    }

    if (port_flag != 0)
    {
        uart = port_flag;
    }
    if (irq_flag != 0)
    {
        irq = irq_flag;
    }

    printf("Looking for UART at port 0x%x, irq %i\n", uart, irq);
}

/*
===============
=
= InitPort
=
===============
*/

void InitPort(void)
{
    int mcr;
    int temp;

    //
    // find the irq and io address of the port
    //
    GetUart();

    //
    // init com port settings
    //
    regs.x.ax = 0xf3;           //f3= 9600 n 8 1
    regs.x.dx = comport - 1;
    int86(0x14, &regs, &regs);

    //
    // check for a 16550
    //
    OUTPUT(uart + FIFO_CONTROL_REGISTER, FCR_FIFO_ENABLE + FCR_TRIGGER_04);
    temp = INPUT(uart + INTERRUPT_ID_REGISTER);
    if ((temp & 0xf8) == 0xc0)
    {
        uart_type = UART_16550;
        printf("UART is a 16550\n\n");
    }
    else
    {
        uart_type = UART_8250;
        OUTPUT(uart + FIFO_CONTROL_REGISTER, 0);
        printf("UART is an 8250\n\n");
    }

    //
    // prepare for interrupts
    //
    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER, 0);
    mcr = INPUT(uart + MODEM_CONTROL_REGISTER);
    mcr |= MCR_OUT2;
    mcr &= ~MCR_LOOPBACK;
    OUTPUT(uart + MODEM_CONTROL_REGISTER, mcr);

    INPUT(uart);                // Clear any pending interrupts
    INPUT(uart + INTERRUPT_ID_REGISTER);

    //
    // hook the irq vector
    //
    irqintnum = irq + 8;

    oldirqvect = getvect(irqintnum);
    setvect(irqintnum, ISR8250);

    OUTPUT(0x20 + 1, INPUT(0x20 + 1) & ~(1 << irq));

    CLI();

    // enable RX and TX interrupts at the uart

    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER,
           IER_RX_DATA_READY + IER_TX_HOLDING_REGISTER_EMPTY);

    // enable interrupts through the interrupt controller

    OUTPUT(0x20, 0xc2);

    // set DTR
    OUTPUT(uart + MODEM_CONTROL_REGISTER,
           INPUT(uart + MODEM_CONTROL_REGISTER) | MCR_DTR);

    STI();

}

/*
=============
=
= ShutdownPort
=
=============
*/

void ShutdownPort(void)
{
    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER, 0);
    OUTPUT(uart + MODEM_CONTROL_REGISTER, 0);

    OUTPUT(0x20 + 1, INPUT(0x20 + 1) | (1 << irq));

    setvect(irqintnum, oldirqvect);

    //
    // init com port settings to defaults
    //
    regs.x.ax = 0xf3;           //f3= 9600 n 8 1
    regs.x.dx = comport - 1;
    int86(0x14, &regs, &regs);
}

int ReadByte(void)
{
    int c;

    if (inque.tail >= inque.head)
        return -1;
    c = inque.data[inque.tail % QUESIZE];
    inque.tail++;
    return c;
}

void WriteByte(int c)
{
    outque.data[outque.head % QUESIZE] = c;
    outque.head++;
}

//==========================================================================

/*
==============
=
= ISR8250
=
==============
*/

void interrupt ISR8250(void)
{
    int c;
    int count;

    while (1)
    {
        switch (INPUT(uart + INTERRUPT_ID_REGISTER) & 7)
        {
            // not enabled
        case IIR_MODEM_STATUS_INTERRUPT:
            modem_status = INPUT(uart + MODEM_STATUS_REGISTER);
            break;

            // not enabled
        case IIR_LINE_STATUS_INTERRUPT:
            line_status = INPUT(uart + LINE_STATUS_REGISTER);
            break;

            //
            // transmit
            //
        case IIR_TX_HOLDING_REGISTER_INTERRUPT:
            //I_ColorBlack (63,0,0);
            if (outque.tail < outque.head)
            {
                if (uart_type == UART_16550)
                    count = 16;
                else
                    count = 1;
                do
                {
                    c = outque.data[outque.tail % QUESIZE];
                    outque.tail++;
                    OUTPUT(uart + TRANSMIT_HOLDING_REGISTER, c);
                } while (--count && outque.tail < outque.head);
            }
            break;

            //
            // receive
            //
        case IIR_RX_DATA_READY_INTERRUPT:
            //I_ColorBlack (0,63,0);
            do
            {
                c = INPUT(uart + RECEIVE_BUFFER_REGISTER);
                inque.data[inque.head % QUESIZE] = c;
                inque.head++;
            } while (uart_type == UART_16550
                     && INPUT(uart + LINE_STATUS_REGISTER) & LSR_DATA_READY);

            break;

            //
            // done
            //
        default:
            //I_ColorBlack (0,0,0);
            OUTPUT(0x20, 0x20);
            return;
        }
    }
}

/*
===============
=
= JumpStart
=
= Start up the transmition interrupts by sending the first char
===============
*/

void JumpStart(void)
{
    int c;

    if (outque.tail < outque.head)
    {
        c = outque.data[outque.tail % QUESIZE];
        outque.tail++;
        OUTPUT(uart, c);
    }
}
