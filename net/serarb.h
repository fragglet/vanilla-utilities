
#include <time.h>

struct arbitration_state {
    doomcom_t *doomcom;
    void (*net_cmd)(void);
    char localid[7];
    clock_t last_send_time;
    int localstage;
    int new_protocol;
};

void StartArbitratePlayers(struct arbitration_state *state, doomcom_t *dc,
                           void (*net_cmd)(void));
void PollArbitratePlayers(struct arbitration_state *state);
int ArbitrationComplete(struct arbitration_state *state);
void ArbitratePlayers(doomcom_t *dc, void (*net_cmd)(void));
void RegisterArbitrationFlags(void);

