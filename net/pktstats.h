//
// Copyright(C) 2025 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#ifndef VUTILS_NET_PKTSTATS_H
#define VUTILS_NET_PKTSTATS_H

struct counter
{
    const char *name;
    unsigned long i;
    struct counter *next;
};

#define DECLARE_COUNTER(name) \
    static struct counter stats_ ## name = { #name, 0, 0 }
#define REGISTER_COUNTER(name) \
    RegisterCounter(&stats_ ## name)
#define INCREMENT_COUNTER(name) \
    ++stats_ ## name.i

void RegisterCounter(struct counter *ctr);
void PacketStatsRegisterFlags(void);

#endif /* #ifndef VUTILS_NET_PKTSTATS_H */
