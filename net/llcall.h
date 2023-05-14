
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

