#ifndef MP_UCF_H
#define MP_UCF_H
#include <melee/ft/forward.h>
void mp_ucf_reset(void);
/* Which UCF runs: 84 for UCF 0.84 (what Slippi runs now, and the default),
 * 80 for UCF 0.8 (what Slippi 3.14 ran, for its replays), 0 for none. */
void mp_ucf_set_version(int version);
int mp_ucf_version(void);
void mp_ucf_input(Fighter*);
void mp_ucf_dashback(Fighter*);
int mp_ucf_sdi(Fighter*);
int mp_ucf_shield_sdi(Fighter*);
int mp_ucf_tumble(Fighter*);
int mp_ucf_drop_extended(Fighter*);
int mp_ucf_suppress_spotdodge(Fighter*);
float mp_ucf_squat_threshold(Fighter*);
#endif
