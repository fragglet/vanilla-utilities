
union ipx_regs {
    struct {
        uint16_t ax, bx, cx, dx;
        uint16_t es, si;
    } x;
    struct {
        uint8_t al, ah, bl, bh, cl, ch, dl, dh;
    } h;
};

extern union ipx_regs ipx_regs;
extern void far (*ipx_entrypoint)();

extern void __stdcall NewIPXCall(void);

