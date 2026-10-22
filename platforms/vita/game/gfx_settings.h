/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MELEE_VITA_GFX_SETTINGS_H
#define MELEE_VITA_GFX_SETTINGS_H

#include <stdbool.h>

/* Graphics options from ux0:data/melee/graphics.txt. The LiveArea Settings
 * button edits them (settings_app.c), the game reads them once at boot.
 * Performance and Quality are fixed sets of values. Changing any value
 * away from them turns the preset into Custom; the Custom values are kept
 * so picking Custom again brings them back. */
typedef struct GfxSettings {
    int menu_res;     /* MELEE_VITA_RESOLUTION_* */
    int game_res;
    int boost;        /* 444/222/222/166 MHz clocks */
    int particles;    /* max particles drawn per list */
    int live_screens; /* refresh copied screens every N frames */
    int vsync;
    int frame_rate;   /* FRAME_RATE_*, see below */
    /* not part of any preset: */
    int show_fps;
    int heavy30;      /* 3+ players or Fountain of Dreams: lock that match to 30 */
    int unlock_all;   /* every character and stage, never saved to the card */
    int tournament;   /* competitive VS rules the first time VS opens each boot */
    int stage_lite;   /* per-stage cutbacks, bit i = gfx_stages[i] */
} GfxSettings;

/* Legal stages that have a cutback switch (stage_lite bits). */
#define GFX_STAGE_COUNT 6
extern const struct GfxStage { int kind; const char* name; } gfx_stages[GFX_STAGE_COUNT];

/* 30 fps lock: game logic still runs at 60 Hz, every 2nd tick is drawn and
 * each frame stays up for exactly 2 refreshes. */
enum { FRAME_RATE_60, FRAME_RATE_30 };
extern int g_melee_vita_fps30_active; /* what's running right now */

enum {
    GFX_PRESET_PERFORMANCE,
    GFX_PRESET_QUALITY,
    GFX_PRESET_CUSTOM,
    GFX_PRESET_COUNT,
};

extern int g_gfx_preset;          /* GFX_PRESET_* */
extern GfxSettings g_gfx_custom;  /* the user's Custom values */
extern GfxSettings g_gfx;         /* what the game uses (preset or custom) */

void gfx_settings_load(void);
bool gfx_settings_save(void); /* saves g_gfx_preset + g_gfx_custom */
/* fixed values for PERFORMANCE / QUALITY, starting values for CUSTOM */
void gfx_settings_preset(GfxSettings* s, int preset);
/* settings for a preset: fixed values, or `custom` for CUSTOM. The options
 * that aren't part of a preset (show_fps, heavy30) always come from `custom`. */
void gfx_settings_resolve(GfxSettings* out, int preset, const GfxSettings* custom);
int gfx_settings_match(const GfxSettings* s); /* preset with these values, or CUSTOM */
void gfx_settings_changed(void);              /* g_gfx edited: update preset/custom */
void gfx_settings_select(int preset);         /* load a preset into g_gfx */
const char* gfx_settings_preset_name(int preset);

/* Push g_gfx into the game: clocks, particle limit, live screen rate. */
void gfx_settings_apply(void);

/* gmvs.c: a match (VS, sudden death, training) starts / ends. With heavy30
 * on and a 60 fps setting, 3+ players or Fountain of Dreams lock that match
 * to 30; the rate never changes mid-match. */
void melee_vita_match_begin(int players, int stage_kind);
void melee_vita_match_end(void);

/* FPS counter. tick() runs on the game thread once per presented frame,
 * draw() on the render thread inside a vita2d scene. */
void gfx_fps_tick(void);
void gfx_fps_draw(void);
struct vita2d_pgf* gfx_font(void);

/* LiveArea Settings screen (settings_app.c). run() never returns. */
bool settings_app_requested(void);
void settings_app_run(void);

#endif
