// Winsock2 patcher program.
//
// Why is this needed? Because WSOCK2.VXD has a bug in the way that it
// translates 16-bit memory addresses to 32-bit linear addresses in kernel
// space. The recv() and send() functions try to translate one too many
// addresses. The result is that these functions always result in a
// WSAENOTSOCK error. Thanks go to Berczi Gabor and his ws2dos.txt for
// documenting this bug and for providing a fix. His workaround recommends
// modifying WSOCK2.VXD and restarting the computer, but this program
// rewrites kernel memory to avoid a restart or modifying system files.
//
// Uh, if you're reading this code then you might in disbelief at the idea
// that this can possibly work. The answer is that yes, it really does work.
// Windows 9x runs 32-bit DOS programs with Windows kernel memory mapped in
// within the C0000000+ range of memory.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <i86.h>
#include <stdint.h>

#define KERNEL_MEM_START  0xc0001000UL
#define VXD_ID_WSOCK2     0x3b0a

#pragma pack(push, 1)
struct ddb
{
    uint32_t next_ddb;
    uint16_t version;
    uint16_t vxd_id;
    uint8_t mver;
    uint8_t minor_version;
    uint16_t flags;
    uint8_t name[8];
    uint32_t init;
    uint32_t control_proc;
    uint32_t v86_proc;
    uint32_t pm_proc;
    uint32_t v86;
    uint32_t pm;
    uint32_t data;
    uint32_t service_size;
    uint32_t win32_ptr;
};
#pragma pack(pop)

extern uint16_t DS(void);
#pragma aux DS = \
    "mov ax, ds" \
    value [ax];

extern uint8_t GetPriv(uint16_t sel);
#pragma aux GetPriv = \
    "movzx eax, bx" \
    "and al, 3" \
    parm [bx] \
    value [al];

uint16_t AllocDescriptor(void)
{
    union REGS regs = {0};
    regs.w.ax = 0;
    regs.w.cx = 1;
    int386(0x31, &regs, &regs);
    if ((regs.w.cflag & 1) != 0)
    {
        fprintf(stderr, "AllocDescriptor failed\n");
        exit(1);
    }
    return regs.w.ax;
}

void SetSegmentAccess(uint16_t sel, uint16_t access)
{
    union REGS regs = {0};

    regs.w.ax = 9;
    regs.w.bx = sel;
    regs.w.cx = (access & 0xff9f) | (GetPriv(sel) << 5);
    int386(0x31, &regs, &regs);
    if ((regs.w.cflag & 1) != 0)
    {
        fprintf(stderr, "SetSegmentAccess failed\n");
        exit(1);
    }
}

void SetSegmentBase(uint16_t sel, uint32_t base)
{
    union REGS regs = {0};

    regs.w.ax = 7;
    regs.w.bx = sel;
    regs.w.cx = (uint16_t) (base >> 16);
    regs.w.dx = (uint16_t) (base & 0xffff);
    int386(0x31, &regs, &regs);
    if ((regs.w.cflag & 1) != 0)
    {
        fprintf(stderr, "SetSegmentBase failed\n");
        exit(1);
    }
}

void SetSegmentLimit(uint16_t sel, uint32_t limit)
{
    union REGS regs = {0};

    regs.w.ax = 8;
    regs.w.bx = sel;
    regs.w.cx = (uint16_t) (limit >> 16);
    regs.w.dx = (uint16_t) (limit & 0xffff);
    int386(0x31, &regs, &regs);
    if ((regs.w.cflag & 1) != 0)
    {
        fprintf(stderr, "SetSegmentLimit failed\n");
        exit(1);
    }
}

void PrintVxD(struct ddb far *ddb)
{
    char buf[9];
    _fmemcpy(buf, ddb->name, 8);
    buf[8] = '\0';
    printf("%s %04x %04x %08x %08x %08x %08x\n",
           buf, ddb->vxd_id, ddb->version, ddb->data,
           ddb->service_size, ddb->pm_proc, ddb->init);
}

struct ddb far *FindVxD(uint16_t sel, struct ddb far *head, uint16_t vxd_id)
{
    struct ddb far *ddb = head;

    for (;;)
    {
        //PrintVxD(ddb);

        if (ddb->vxd_id == vxd_id)
        {
            return ddb;
        }

        if (ddb->next_ddb == 0)
        {
            return NULL;
        }
        ddb = MK_FP(sel, ddb->next_ddb);
    }
}

void Hexdump(uint8_t far *addr)
{
    for (;;)
    {
        if ((((unsigned long) addr) & 0xf) == 0)
        {
            printf("%08x: ", (unsigned long) addr);
        }
        printf(" %02x", *addr);
        if ((((unsigned long) addr) & 0xf) == 15)
        {
            printf("\n");
        }
        ++addr;
    }
}

// Our version of the wrong[] array below deliberately stores the first byte
// with the wrong value. This is in case we are somehow(?) reading from our own
// memory; we don't want to overwrite the wrong[] array.
#define WRONG_FIRST_BYTE  0x01

uint8_t wrong[] = { 0xff, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00,
                    0x00, 0x04, 0x03, 0x03, 0x00, 0x03 };
uint8_t right[] = { 0xff, 0x01, 0x00, 0x01, 0x01, 0x01, 0x01, 0x00,
                    0x00, 0x03, 0x03, 0x03, 0x00, 0x02 };
//                     recv ^^                 send ^^

static uint8_t far *FindByteSequence(
    uint8_t far *haystack, uint8_t needle_first,
    uint8_t *needle, size_t needle_len)
{
    uint8_t far *addr;

    // TODO: It would be nice if there was some way to determine when
    // we've reached unreadable memory; currently we just crash.
    for (addr = haystack;; addr++)
    {
        if (*addr == needle_first
         && !_fmemcmp(addr + 1, needle + 1, needle_len - 1))
        {
            return addr;
        }
    }
    // Never reached.
    return NULL;
}

int main()
{
    uint16_t sel;
    uint8_t far *addr;
    struct ddb far *ddb;

    printf("Winsock2 VxD patcher.\n"
           "This program applies a hotfix to Win9x kernel memory to fix\n"
           "a bug in WSOCK2.VXD.\n\n");

    // TODO: Check Windows version and that we're on Win9x.
    // TODO: Use VXDLDR to ensure WSOCK2.VXD is loaded first.

    // Thanks to joncampbell123, this is essentially stolen (but
    // rewritten) from doslib.
    sel = AllocDescriptor();
    SetSegmentAccess(sel, 0x90 | ((DS() & 3) << 5) | 0x02);  // R/W
    SetSegmentBase(sel, 0);
    SetSegmentLimit(sel, 0xFFFFFFFF);

    printf("Looking for the head of the VxD chain...\n");
    addr = FindByteSequence(MK_FP(sel, KERNEL_MEM_START),
                            'V', "xMM     ", 8) - 12;

    printf("Found the VxD chain head at %lu, looking for WSOCK2 VxD...\n",
           (unsigned long) addr);
    ddb = FindVxD(sel, (struct ddb far *) addr, VXD_ID_WSOCK2);
    if (ddb == NULL)
    {
        fprintf(stderr, "Searched VxD chain, can't find WSOCK2.\n"
                        "Check (on Win95) that the Winsock 2 Update is "
                        "installed; if you just\nbooted the system, try "
                        "pinging something to load the VxD.\n\n");
        exit(1);
    }

    // We use ddb->pm_proc as a pointer into the WSOCK2.VXD code to
    // start looking for the table.
    printf("Found WSOCK2 VxD at %lu, searching for buggy table.\n",
           (unsigned long) ddb);
    printf("If the program crashes, the table may be patched already.\n");
    //Hexdump(MK_FP(sel, ddb->pm_proc));
    addr = FindByteSequence(MK_FP(sel, ddb->pm_proc), WRONG_FIRST_BYTE,
                            wrong, sizeof(wrong));

    printf("Found buggy table at %lu, applying patch...\n",
           (unsigned long) addr);
    _fmemcpy(addr + 1, right + 1, sizeof(right) - 1);

    if (_fmemcmp(addr + 1, right + 1, sizeof(right) - 1) != 0)
    {
        fprintf(stderr, "Write to kernel memory failed.\n");
        exit(1);
    }
    printf("Patch was applied to kernel memory successfully.\n"
           "16-bit DOS programs should now be able to use Winsock2.\n");

    return 0;
}
