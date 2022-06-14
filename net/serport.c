#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/serport.h"

void JumpStart(void);

static void interrupt far ISR8250(void);
static void interrupt far ISR16550(void);

static union REGS regs;
static struct SREGS sregs;

que_t inque, outque;

int uart;                       // io address
static enum { UART_8250, UART_16550 } uart_type;
int irq;

static int modem_status = -1;
static int line_status = -1;

static void (interrupt far *oldirqvect) (void);
static int irqintnum;

static int comport;

// Flags:
static int com2 = 0, com3 = 0, com4 = 0;
static int port_flag = 0, irq_flag = 0;
static int force_8250 = 0;
static int baud_9600 = 0, baud_14400 = 0, baud_19200 = 0;
static int baud_38400 = 0, baud_57600 = 0, baud_115200 = 0;

void SerialRegisterFlags(void)
{
    BoolFlag("-com2", &com2, "(and -com3, -com4) use COMx instead of COM1");
    BoolFlag("-com3", &com3, NULL);
    BoolFlag("-com4", &com4, NULL);

    BoolFlag("-9600", &baud_9600,
             "(and -14400, -19200, -38400, -57600, -115200) baud rate");
    BoolFlag("-14400", &baud_14400, NULL);
    BoolFlag("-19200", &baud_19200, NULL);
    BoolFlag("-38400", &baud_38400, NULL);
    BoolFlag("-57600", &baud_57600, NULL);
    BoolFlag("-115200", &baud_115200, NULL);

    BoolFlag("-8250", &force_8250, NULL);
    IntFlag("-port", &port_flag, "port", NULL);
    IntFlag("-irq", &irq_flag, "irq", NULL);
}

void GetUart(void)
{
    char far *system_data;
    char portname[5];
    static int ISA_uarts[] = { 0x3f8, 0x2f8, 0x3e8, 0x2e8 };
    static int ISA_IRQs[] = { 4, 3, 4, 3 };
    static int MCA_uarts[] = { 0x03f8, 0x02f8, 0x3220, 0x3228 };
    static int MCA_IRQs[] = { 4, 3, 3, 3 };

    if (com2)
    {
        comport = 2;
    }
    else if (com3)
    {
        comport = 3;
    }
    else if (com4)
    {
        comport = 4;
    }
    else
    {
        comport = 1;
    }

    regs.h.ah = 0xc0;
    int86x(0x15, &regs, &regs, &sregs);
    if (regs.x.cflag != 0)
    {
        irq = ISA_IRQs[comport - 1];
        uart = ISA_uarts[comport - 1];
        return;
    }

    system_data = (char far *)(((long)sregs.es << 16) + regs.x.bx);
    if ((system_data[5] & 0x02) != 0)
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

    sprintf(portname, "COM%d", comport);
    SetLogDistinguisher(portname);
    LogMessage("Looking for UART at port 0x%x, irq %i", uart, irq);
}

static long OverrideBaudRate(long baudrate)
{
    int i;
    struct {
        int *flag;
        long baud;
    } bauds[] = {
        {&baud_9600,   9600L},
        {&baud_14400,  14400L},
        {&baud_19200,  19200L},
        {&baud_38400,  38400L},
        {&baud_57600,  57600L},
        {&baud_115200, 115200L},
    };

    for (i = 0; i < sizeof(bauds) / sizeof(*bauds); ++i)
    {
        if (*bauds[i].flag)
        {
            return bauds[i].baud;
        }
    }

    return baudrate;
}

void InitPort(long baudrate)
{
    int baudbits;
    int temp;
    int u;

    // Allow baud rate override via command line:
    baudrate = OverrideBaudRate(baudrate);

    // find the irq and i/o address of the port
    GetUart();

    // disable all uart interrupts
    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER, 0);

    // init com port settings
    LogMessage("Setting port to %lu baud", baudrate);

    // set baud rate
    baudbits = 115200L / baudrate;
    OUTPUT(uart + LINE_CONTROL_REGISTER, 0x83);
    OUTPUT(uart, baudbits);
    OUTPUT(uart + 1, 0);

    // set line control register (N81)
    OUTPUT(uart + LINE_CONTROL_REGISTER, 0x03);

    // set modem control register (OUT2+RTS+DTR)
    OUTPUT(uart + MODEM_CONTROL_REGISTER, 8 + 2 + 1);

    // check for a 16550
    if (force_8250)
    {
        // allow a forced 8250
        uart_type = UART_8250;
    }
    else
    {
        OUTPUT(uart + FIFO_CONTROL_REGISTER, FCR_FIFO_ENABLE + FCR_TRIGGER_04);
        temp = INPUT(uart + INTERRUPT_ID_REGISTER);
        if ((temp & 0xf8) == 0xc0)
        {
            uart_type = UART_16550;
        }
        else
        {
            uart_type = UART_8250;
            OUTPUT(uart + FIFO_CONTROL_REGISTER, 0);
        }
    }

    // clear out any pending uart events: clear an entire 16550 silo
    for (u = 0; u < 16; u++)
    {
        INPUT(uart + RECEIVE_BUFFER_REGISTER);
    }

    do
    {
        u = INPUT(uart + INTERRUPT_ID_REGISTER) & 7;
        switch (u)
        {
            case IIR_MODEM_STATUS_INTERRUPT:
                modem_status = INPUT(uart + MODEM_STATUS_REGISTER);
                break;

            case IIR_LINE_STATUS_INTERRUPT:
                line_status = INPUT(uart + LINE_STATUS_REGISTER);
                break;

            case IIR_TX_HOLDING_REGISTER_INTERRUPT:
                break;

            case IIR_RX_DATA_READY_INTERRUPT:
                INPUT(uart + RECEIVE_BUFFER_REGISTER);
                break;
        }
    } while (!(u & 1));

    // hook the irq vector
    irqintnum = irq + 8;

    oldirqvect = _dos_getvect(irqintnum);
    if (uart_type == UART_16550)
    {
        _dos_setvect(irqintnum, ISR16550);
        LogMessage("UART = 16550");
    }
    else
    {
        _dos_setvect(irqintnum, ISR8250);
        LogMessage("UART = 8250");
    }

    OUTPUT(0x20 + 1, INPUT(0x20 + 1) & ~(1 << irq));

    _disable();

    // enable RX and TX interrupts at the uart
    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER,
           IER_RX_DATA_READY + IER_TX_HOLDING_REGISTER_EMPTY);

    // enable interrupts through the interrupt controller
    OUTPUT(0x20, 0xc2);

    _enable();
}

void ShutdownPort(void)
{
    int u;

    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER, 0);
    OUTPUT(uart + MODEM_CONTROL_REGISTER, 0);

    // clear an entire 16550 silo
    for (u = 0; u < 16; u++)
    {
        INPUT(uart + RECEIVE_BUFFER_REGISTER);
    }

    OUTPUT(0x20 + 1, INPUT(0x20 + 1) | (1 << irq));

    _dos_setvect(irqintnum, oldirqvect);

    // init com port settings to defaults
    regs.x.ax = 0xf3;           //f3= 9600 n 8 1
    regs.x.dx = comport - 1;
    int86(0x14, &regs, &regs);
}

int ReadByte(void)
{
    int c;

    if (inque.tail >= inque.head)
    {
        return -1;
    }
    c = inque.data[inque.tail & (QUESIZE - 1)];
    inque.tail++;
    return c;
}

void WriteByte(int c)
{
    outque.data[outque.head & (QUESIZE - 1)] = c;
    outque.head++;
}

static void interrupt far ISR8250(void)
{
    int c;

    while (1)
    {
        switch (INPUT(uart + INTERRUPT_ID_REGISTER) & 7)
        {
            case IIR_MODEM_STATUS_INTERRUPT:
                // not enabled
                modem_status = INPUT(uart + MODEM_STATUS_REGISTER);
                break;

            case IIR_LINE_STATUS_INTERRUPT:
                // not enabled
                line_status = INPUT(uart + LINE_STATUS_REGISTER);
                break;

            case IIR_TX_HOLDING_REGISTER_INTERRUPT:
                // transmit
                if (outque.tail < outque.head)
                {
                    c = outque.data[outque.tail & (QUESIZE - 1)];
                    outque.tail++;
                    OUTPUT(uart + TRANSMIT_HOLDING_REGISTER, c);
                }
                break;

            case IIR_RX_DATA_READY_INTERRUPT:
                // receive
                c = INPUT(uart + RECEIVE_BUFFER_REGISTER);
                inque.data[inque.head & (QUESIZE - 1)] = c;
                inque.head++;
                break;

            default:
                // done
                OUTPUT(0x20, 0x20);
                return;
        }
    }
}

static void interrupt far ISR16550(void)
{
    int c;
    int count;

    while (1)
    {
        switch (INPUT(uart + INTERRUPT_ID_REGISTER) & 7)
        {
            case IIR_MODEM_STATUS_INTERRUPT:
                // not enabled
                modem_status = INPUT(uart + MODEM_STATUS_REGISTER);
                break;

            case IIR_LINE_STATUS_INTERRUPT:
                // not enabled
                line_status = INPUT(uart + LINE_STATUS_REGISTER);
                break;

            case IIR_TX_HOLDING_REGISTER_INTERRUPT:
                // transmit
                count = 16;
                while (outque.tail < outque.head && count--)
                {
                    c = outque.data[outque.tail & (QUESIZE - 1)];
                    outque.tail++;
                    OUTPUT(uart + TRANSMIT_HOLDING_REGISTER, c);
                }
                break;

            case IIR_RX_DATA_READY_INTERRUPT:
                // receive
                do
                {
                    c = INPUT(uart + RECEIVE_BUFFER_REGISTER);
                    inque.data[inque.head & (QUESIZE - 1)] = c;
                    inque.head++;
                } while (INPUT(uart + LINE_STATUS_REGISTER) & LSR_DATA_READY);

                break;

            default:
                // done
                OUTPUT(0x20, 0x20);
                return;
        }
    }
}

// Start up the transmition interrupts by sending the first char
void JumpStart(void)
{
    int c;

    if (outque.tail < outque.head)
    {
        c = outque.data[outque.tail & (QUESIZE - 1)];
        outque.tail++;
        OUTPUT(uart, c);
    }
}
