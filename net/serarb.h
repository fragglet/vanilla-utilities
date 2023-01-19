
void StartArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void));
int PollArbitratePlayers(void);
void ArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void));
void RegisterArbitrationFlags(void);

extern int force_player1, force_player2;

