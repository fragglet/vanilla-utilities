//
// Copyright(C) 1993 id Software, Inc.
// Copyright(C) 2019-2023 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <bios.h>
#include "lib/inttypes.h"

#include "lib/dos.h"
#include "lib/flag.h"
#include "lib/ints.h"
#include "lib/log.h"
#include "net/doomnet.h"
#include "net/serport.h"

static void interrupt far ISR8250(void);
static void interrupt far ISR16550(void);

uint8_t serial_tx_buffer[SERIAL_TX_BUFFER_LEN];
static unsigned int serial_tx_index, serial_tx_bytes;
static int transmitting = 0, receiving = 1;

static union REGS regs;
static struct SREGS sregs;

static struct irq_hook uart_interrupt;

int uart;                       // io address
static enum { UART_8250, UART_16550 } uart_type;
static int irq;

static int modem_status = -1;
static int line_status = -1;

static int comport;

// Flags:
static int com2 = 0, com3 = 0, com4 = 0;
static int port_flag = 0, irq_flag = 0;
static int force_8250 = 0;
static int baud_9600 = 0, baud_14400 = 0, baud_19200 = 0;
static int baud_38400 = 0, baud_57600 = 0, baud_115200 = 0;

void SerialRegisterFlags(void)
{
    BoolFlag("-com2", &com2, "(or -com3/-com4) use COMx instead of COM1");
    BoolFlag("-com3", &com3, NULL);
    BoolFlag("-com4", &com4, NULL);

    BoolFlag("-9600", &baud_9600,
             "(or -14400/-19200/-38400/-57600/-115200) set baud rate");
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

    if (comport > ((_bios_equiplist() >> 9) & 0x7))
    {
        LogMessage("Proceeding, but your BIOS says you don't have a COM%d.",
                   comport);
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

    // hook the IRQ vector
    if (uart_type == UART_16550)
    {
        HookIRQ(&uart_interrupt, ISR16550, irq);
        LogMessage("UART = 16550");
    }
    else
    {
        HookIRQ(&uart_interrupt, ISR8250, irq);
        LogMessage("UART = 8250");
    }

    // enable RX and TX interrupts at the uart
    OUTPUT(uart + INTERRUPT_ENABLE_REGISTER,
           IER_RX_DATA_READY + IER_TX_HOLDING_REGISTER_EMPTY);

    // This was in the original SERSETUP source code and was a bit of a
    // mystery, but I think I figured it out. This is setting OCW2 in the
    // PIC to invoke the "Set Priority" command. Specifically, this sets a
    // specific rotation where channel 2 is the bottom priority. Why?
    // Well, if IRQ2 is the bottom priority, then IRQs 3 and 4 become the
    // top priority, and those are the IRQs used by the COM1 and COM2
    // UARTs. So this potentially gives a little bit of priority boost to
    // the ISR. 
    // Note the original comment attached to this line, "enable interrupts
    // through the interrupt controller", is entirely misleading.
    OUTPUT(0x20, 0xc2);
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

    RestoreIRQ(&uart_interrupt);

    // init com port settings to defaults
    regs.x.ax = 0xf3;           //f3= 9600 n 8 1
    regs.x.dx = comport - 1;
    int86(0x14, &regs, &regs);
}

static inline int RefillTXBuffer(void)
{
    serial_tx_bytes = SerialMoreTXData();
    serial_tx_index = 0;
    return serial_tx_bytes != 0;
}

static inline void TransmitNextByte(void)
{
    int c = serial_tx_buffer[serial_tx_index];
    ++serial_tx_index;
    OUTPUT(uart + TRANSMIT_HOLDING_REGISTER, c);
    transmitting = 1;
}

// DisableReceive is called when SerialByteReceived returns failure,
// which indicates there is no more buffer space to store data. This may
// indicate an overload situation, or simply that the GetPacket function is
// not being regularly called by DOOM (which happens during the startup
// process). Either way, it is useless to invoke the ISR until buffer space
// has been freed up, so we rewrite the IER to disable rx notifications.
static inline void DisableReceive(void)
{
    if (receiving)
    {
        receiving = 0;
        OUTPUT(uart + INTERRUPT_ENABLE_REGISTER,
               IER_TX_HOLDING_REGISTER_EMPTY);
    }
}

static void interrupt far ISR8250(void)
{
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
                while (INPUT(uart + LINE_STATUS_REGISTER) & LSR_THRE)
                {
                    if (serial_tx_index >= serial_tx_bytes && !RefillTXBuffer())
                    {
                        transmitting = 0;
                        break;
                    }
                    TransmitNextByte();
                }
                break;

            case IIR_RX_DATA_READY_INTERRUPT:
                // receive
                if (!SerialByteReceived(INPUT(uart + RECEIVE_BUFFER_REGISTER)))
                {
                    DisableReceive();
                }
                break;

            default:
                // done
                EndOfIRQ(&uart_interrupt);
                return;
        }
    }
}

void PollSerialReceive(void)
{
    while (INPUT(uart + LINE_STATUS_REGISTER) & LSR_DATA_READY)
    {
        if (!SerialByteReceived(INPUT(uart + RECEIVE_BUFFER_REGISTER)))
        {
            DisableReceive();
            break;
        }
    }
}

static void interrupt far ISR16550(void)
{
    int done = 0;
    int count;

    while (!done)
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
                for (count = 0; count < 16; count++)
                {
                    if (serial_tx_index >= serial_tx_bytes && !RefillTXBuffer())
                    {
                        transmitting = 0;
                        break;
                    }
                    TransmitNextByte();
                }
                break;

            case IIR_RX_DATA_READY_INTERRUPT:
                // receive
                PollSerialReceive();
                break;

            default:
                done = 1;
                break;
        }
    }

    EndOfIRQ(&uart_interrupt);
}

// Start up the transmission interrupts by sending the first char
void JumpStart(void)
{
    if (transmitting)
    {
        return;
    }
    if (serial_tx_index < serial_tx_bytes || RefillTXBuffer())
    {
        TransmitNextByte();
    }
}

// Invoked when new rx buffer space has been made available; if we previously
// turned off receive, we can now rewrite the IER to turn rx notifications
// back on again.
void ResumeReceive(void)
{
    if (!receiving)
    {
        receiving = 1;
        OUTPUT(uart + INTERRUPT_ENABLE_REGISTER,
               IER_RX_DATA_READY + IER_TX_HOLDING_REGISTER_EMPTY);
        PollSerialReceive();
    }
}

