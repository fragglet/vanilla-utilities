//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

void FlushPendingPackets(void);
void AggregatedSendPacket(int node, void *data, size_t data_len);
void InitAggregation(int numnodes,
                     void (*send)(int node, void *data, size_t data_len));

