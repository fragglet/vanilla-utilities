//
// Copyright 1994 Scott Coleman, American Society of Reverse Engineers
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 1.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

// Declarations for the DOOM parallel port driver.
void ParallelRegisterFlags(void);
void InitPort(void);
void ShutdownPort(void);
unsigned int NextPacket(uint8_t *result_buf, unsigned int max_len);

