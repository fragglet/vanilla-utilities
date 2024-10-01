//
// Copyright(C) 2019-2023 Simon Howard
//
// You can redistribute and/or modify this program under the terms of the
// GNU General Public License version 2 as published by the Free Software
// Foundation, or any later version. This program is distributed WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.
//

void StartArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void));
int PollArbitratePlayers(void);
void ArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void));
void RegisterArbitrationFlags(void);

extern int force_player1, force_player2;

