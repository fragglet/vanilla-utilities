//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

extern int nodetoplayer[MAXNETNODES];
extern int playertonode[MAXPLAYERS];

int CheckLateDiscover(doomcom_t far *doomcom);
void DiscoverPlayers(doomcom_t far *doomcom);

