/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MELEE_VITA_GFX_SETTINGS_H
#define MELEE_VITA_GFX_SETTINGS_H

#include <stdbool.h>

/* Graphics options from ux0:data/melee/graphics.txt. The LiveArea Settings
 * button edits them (settings_app.c), the game reads them once at boot. */
typedef struct GfxSettings {
    int menu_res;     /* MELEE_VITA_RESOLUTION_* */
    int game_res;
    int boost;        /* 444/222/222/166 MHz clocks */
    int particles;    /* max particles drawn per list */
    int live_screens; /* refresh copied screens every N frames */
    int vsync;
    int show_fps;
    int fps30;        /* lock to 30 fps: logic still 60 Hz, draw every 2nd tick */
} GfxSettings;

enum {
    GFX_PRESET_PERFORMANCE,
    GFX_PRESET_DEFAULT,
    GFX_PRESET_QUALITY,
    GFX_PRESET_COUNT,
    GFX_PRESET_CUSTOM = GFX_PRESET_COUNT,
};

extern GfxSettings g_gfx;

void gfx_settings_load(void);
bool gfx_settings_save(void);
void gfx_settings_preset(GfxSettings* s, int preset);
int gfx_settings_match_preset(const GfxSettings* s);
const char* gfx_settings_preset_name(int preset);

/* Push g_gfx into the game: clocks, particle limit, live screen rate. */
void gfx_settings_apply(void);

/* FPS counter. tick() runs on the game thread once per presented frame,
 * draw() on the render thread inside a vita2d scene. */
void gfx_fps_tick(void);
void gfx_fps_draw(void);
struct vita2d_pgf* gfx_font(void);

/* LiveArea Settings screen (settings_app.c). run() never returns. */
bool settings_app_requested(void);
void settings_app_run(void);

#endif
