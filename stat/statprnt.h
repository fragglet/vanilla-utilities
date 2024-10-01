//
// Copyright(C) 2007-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

#ifndef STATPRNT_H
#define STATPRNT_H

void DiscoverGamemode(wbstartstruct_t *stats, int num_stats);
void PrintStats(FILE *stream, wbstartstruct_t *stats);

#endif                          /* #ifndef STATPRNT_H */
