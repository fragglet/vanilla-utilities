//
// Copyright(C) 1993 id Software, Inc.
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#define TRANSMIT_HOLDING_REGISTER            0x00
#define RECEIVE_BUFFER_REGISTER              0x00
#define INTERRUPT_ENABLE_REGISTER            0x01
#define   IER_RX_DATA_READY                  0x01
#define   IER_TX_HOLDING_REGISTER_EMPTY      0x02
#define   IER_LINE_STATUS                    0x04
#define   IER_MODEM_STATUS                   0x08
#define INTERRUPT_ID_REGISTER                0x02
#define   IIR_MODEM_STATUS_INTERRUPT         0x00
#define   IIR_TX_HOLDING_REGISTER_INTERRUPT  0x02
#define   IIR_RX_DATA_READY_INTERRUPT        0x04
#define   IIR_LINE_STATUS_INTERRUPT          0x06
#define FIFO_CONTROL_REGISTER                0x02
#define   FCR_FIFO_ENABLE                    0x01
#define   FCR_RCVR_FIFO_RESET                0x02
#define   FCR_XMIT_FIFO_RESET                0x04
#define   FCR_RCVR_TRIGGER_LSB               0x40
#define   FCR_RCVR_TRIGGER_MSB               0x80
#define   FCR_TRIGGER_01                     0x00
#define   FCR_TRIGGER_04                     0x40
#define   FCR_TRIGGER_08                     0x80
#define   FCR_TRIGGER_14                     0xc0
#define LINE_CONTROL_REGISTER                0x03
#define   LCR_WORD_LENGTH_MASK               0x03
#define   LCR_WORD_LENGTH_SELECT_0           0x01
#define   LCR_WORD_LENGTH_SELECT_1           0x02
#define   LCR_STOP_BITS                      0x04
#define   LCR_PARITY_MASK                    0x38
#define   LCR_PARITY_ENABLE                  0x08
#define   LCR_EVEN_PARITY_SELECT             0x10
#define   LCR_STICK_PARITY                   0x20
#define   LCR_SET_BREAK                      0x40
#define   LCR_DLAB                           0x80
#define MODEM_CONTROL_REGISTER               0x04
#define   MCR_DTR                            0x01
#define   MCR_RTS                            0x02
#define   MCR_OUT1                           0x04
#define   MCR_OUT2                           0x08
#define   MCR_LOOPBACK                       0x10
#define LINE_STATUS_REGISTER                 0x05
#define   LSR_DATA_READY                     0x01
#define   LSR_OVERRUN_ERROR                  0x02
#define   LSR_PARITY_ERROR                   0x04
#define   LSR_FRAMING_ERROR                  0x08
#define   LSR_BREAK_DETECT                   0x10
#define   LSR_THRE                           0x20
#define MODEM_STATUS_REGISTER                0x06
#define   MSR_DELTA_CTS                      0x01
#define   MSR_DELTA_DSR                      0x02
#define   MSR_TERI                           0x04
#define   MSR_DELTA_CD                       0x08
#define   MSR_CTS                            0x10
#define   MSR_DSR                            0x20
#define   MSR_RI                             0x40
#define   MSR_CD                             0x80
#define DIVISOR_LATCH_LOW                    0x00
#define DIVISOR_LATCH_HIGH                   0x01

void SerialRegisterFlags(void);
void InitPort(long baudrate);
void ShutdownPort(void);

// SerialByteReceived is called every time a new byte is received from
// the serial port. If it returns zero, no more data will be delivered
// until ResumeReceive is called. This may lead to data being lost, but
// should be done if there is no more buffer space left to store data.
int SerialByteReceived(uint8_t c);

// SerialMoreTXData fills this buffer.
#define SERIAL_TX_BUFFER_LEN  16
extern uint8_t serial_tx_buffer[SERIAL_TX_BUFFER_LEN];

// SerialMoreTXData to refill the transmit buffer when it's empty.
// Returns the number of bytes placed into the buffer, or zero to mean
// "stop transmitting".
unsigned int SerialMoreTXData(void);

// JumpStart restarts transmit if transmit had previously stopped.
void JumpStart(void);

// ResumeReceive is called to restart receive, if SerialByteReceived
// previously returned zero.
void ResumeReceive(void);

