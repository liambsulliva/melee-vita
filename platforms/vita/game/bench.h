/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MELEE_VITA_BENCH_H
#define MELEE_VITA_BENCH_H

#include <dolphin/types.h>

/* Benchmark mode. The Settings screen's "Run benchmark" row relaunches the
 * game, which then plays one CPU match on each stage in bench_stages[] using
 * the title screen's attract demo, records every frame and writes a report
 * to ux0:data/melee/benchmark/. Vita only (not PS5). */

/* main(): picks up the request file left by the Settings screen */
void melee_vita_bench_init(void);
int melee_vita_bench_active(void);

/* game side (gmopeningmode.c / gmtitle.c) */
int melee_vita_bench_stage(void); /* StKind for the next match */
int melee_vita_bench_players(void); /* 2 or 4 */
u32 melee_vita_bench_seed(void);
/* called every logic tick of a benchmark match with the match frame;
 * returns 1 when the match should end */
int melee_vita_bench_match_tick(u32 frame, int cancel);

/* gxm_game.c: once per presented frame, after the render-thread sync */
void melee_vita_bench_frame(u32 sync_wait_us, u32 drain_us);
/* display callback: a flip was queued at this vcount; target = refreshes
 * each frame should stay up (1 at 60 fps, 2 at 30) */
void melee_vita_bench_flip(u32 vcount, u32 target);
/* render thread: banner while running, results once done */
int melee_vita_bench_wants_overlay(void);
void melee_vita_bench_draw(void);

/* settings_app.c: ask for a benchmark on the next launch */
void melee_vita_bench_request(int num_players); /* 2 or 4 */

#endif
