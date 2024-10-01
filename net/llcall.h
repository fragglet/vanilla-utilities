//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
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

