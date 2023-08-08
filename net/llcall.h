//
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

union ll_regs {
    struct {
        uint16_t ax, bx, cx, dx;
        uint16_t es, si;
    } x;
    struct {
        uint8_t al, ah, bl, bh, cl, ch, dl, dh;
    } h;
};

extern union ll_regs ll_regs;
extern void __stdcall far (*ll_funcptr)();

extern void __stdcall LowLevelCall(void);

