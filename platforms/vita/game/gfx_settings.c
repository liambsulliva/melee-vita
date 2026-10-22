/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gfx_settings.h"
#include "gxm_resolution.h"
#include "../vita_log.h"

#include <dolphin/types.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <vita2d.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GFX_PATH "ux0:data/melee/graphics.txt"
#define GFX_TMP_PATH "ux0:data/melee/graphics.tmp"

#ifndef MELEE_VITA_DEFAULT_MENU_RESOLUTION_OPTION
#define MELEE_VITA_DEFAULT_MENU_RESOLUTION_OPTION MELEE_VITA_RESOLUTION_NATIVE
#endif
#ifndef MELEE_VITA_DEFAULT_GAMEPLAY_RESOLUTION_OPTION
#define MELEE_VITA_DEFAULT_GAMEPLAY_RESOLUTION_OPTION \
    MELEE_VITA_DEFAULT_MENU_RESOLUTION_OPTION
#endif

/* these live in gx.c so the PS5 build (which shares gx.c and psdisp.c)
 * keeps the old values */
extern u32 g_melee_vita_particle_limit;
extern u32 g_melee_vita_copy_interval;
extern int g_melee_vita_min_ticks; // gmscene.c, logic ticks per drawn frame

GfxSettings g_gfx;
GfxSettings g_gfx_custom;
int g_gfx_preset = GFX_PRESET_PERFORMANCE;

static const char* preset_names[] = { "PERFORMANCE", "QUALITY", "CUSTOM" };

void gfx_settings_preset(GfxSettings* s, int preset)
{
    memset(s, 0, sizeof(*s));
    if (preset == GFX_PRESET_PERFORMANCE) {
        s->menu_res = MELEE_VITA_RESOLUTION_60;
        s->game_res = MELEE_VITA_RESOLUTION_60;
        s->boost = 1;
        s->particles = 256;
        s->live_screens = 8;
        s->frame_rate = FRAME_RATE_60;
    } else if (preset == GFX_PRESET_QUALITY) {
        s->menu_res = MELEE_VITA_RESOLUTION_NATIVE;
        s->game_res = MELEE_VITA_RESOLUTION_NATIVE;
        s->boost = 1;
        s->particles = 2048;
        s->live_screens = 1;
        s->frame_rate = FRAME_RATE_30;
    } else {
        // Custom starts out like the release build before this menu existed
        s->menu_res = MELEE_VITA_DEFAULT_MENU_RESOLUTION_OPTION;
        s->game_res = MELEE_VITA_DEFAULT_GAMEPLAY_RESOLUTION_OPTION;
        s->boost = 0;
        s->particles = 512;
        s->live_screens = 4;
    }
}

// which preset these values are (show_fps doesn't count), or CUSTOM
int gfx_settings_match(const GfxSettings* s)
{
    for (int p = 0; p < GFX_PRESET_CUSTOM; p++) {
        GfxSettings v;
        gfx_settings_preset(&v, p);
        v.show_fps = s->show_fps; // these two aren't part of a preset
        v.heavy30 = s->heavy30;
        if (memcmp(&v, s, sizeof(v)) == 0) return p;
    }
    return GFX_PRESET_CUSTOM;
}

// g_gfx was edited: a value that differs from the preset makes it Custom
void gfx_settings_changed(void)
{
    g_gfx_preset = gfx_settings_match(&g_gfx);
    if (g_gfx_preset == GFX_PRESET_CUSTOM) g_gfx_custom = g_gfx;
    g_gfx_custom.show_fps = g_gfx.show_fps;
    g_gfx_custom.heavy30 = g_gfx.heavy30;
}

// preset row: load that preset's values (Custom brings back the user's own)
void gfx_settings_select(int preset)
{
    g_gfx_custom.show_fps = g_gfx.show_fps;
    g_gfx_custom.heavy30 = g_gfx.heavy30;
    gfx_settings_resolve(&g_gfx, preset, &g_gfx_custom);
    g_gfx_preset = preset;
}

void gfx_settings_resolve(GfxSettings* out, int preset, const GfxSettings* custom)
{
    if (preset == GFX_PRESET_CUSTOM) {
        *out = *custom;
        return;
    }
    gfx_settings_preset(out, preset);
    out->show_fps = custom->show_fps; // not part of a preset
    out->heavy30 = custom->heavy30;
}

const char* gfx_settings_preset_name(int preset)
{
    if (preset < 0 || preset >= GFX_PRESET_COUNT)
        preset = GFX_PRESET_CUSTOM;
    return preset_names[preset];
}

static int res_to_percent(int res)
{
    switch (res) {
    case MELEE_VITA_RESOLUTION_75: return 75;
    case MELEE_VITA_RESOLUTION_60: return 60;
    case MELEE_VITA_RESOLUTION_50: return 50;
    default: return 100;
    }
}

static int percent_to_res(int pct)
{
    if (pct >= 100) return MELEE_VITA_RESOLUTION_NATIVE;
    if (pct >= 75) return MELEE_VITA_RESOLUTION_75;
    if (pct >= 60) return MELEE_VITA_RESOLUTION_60;
    return MELEE_VITA_RESOLUTION_50;
}

static void clamp_settings(GfxSettings* s)
{
    if (s->menu_res < 0 || s->menu_res >= MELEE_VITA_RESOLUTION_OPTION_COUNT)
        s->menu_res = MELEE_VITA_RESOLUTION_NATIVE;
    if (s->game_res < 0 || s->game_res >= MELEE_VITA_RESOLUTION_OPTION_COUNT)
        s->game_res = MELEE_VITA_RESOLUTION_NATIVE;
    if (s->particles < 16) s->particles = 16;
    if (s->particles > 4096) s->particles = 4096;
    if (s->live_screens < 1) s->live_screens = 1;
    if (s->live_screens > 30) s->live_screens = 30;
    s->boost = s->boost != 0;
    s->vsync = s->vsync != 0;
    s->show_fps = s->show_fps != 0;
    s->heavy30 = s->heavy30 != 0;
    if (s->frame_rate != FRAME_RATE_30) s->frame_rate = FRAME_RATE_60;
}

void gfx_settings_load(void)
{
    char line[128];
    FILE* f;

    GfxSettings* c = &g_gfx_custom;

    gfx_settings_preset(c, GFX_PRESET_CUSTOM);
    c->heavy30 = 1;
    g_gfx_preset = GFX_PRESET_PERFORMANCE; // the default, also for files without a preset line

    f = fopen(GFX_PATH, "r");
    if (!f) {
        gfx_settings_resolve(&g_gfx, g_gfx_preset, c);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        char name[32], str[32];
        int val;
        if (line[0] == '#') continue;
        if (sscanf(line, " %31[^= ] = %31s", name, str) != 2) continue;
        val = atoi(str);

        if (!strcmp(name, "preset"))
            g_gfx_preset = !strcmp(str, "performance") ? GFX_PRESET_PERFORMANCE
                         : !strcmp(str, "quality") ? GFX_PRESET_QUALITY : GFX_PRESET_CUSTOM;
        else if (!strcmp(name, "menu_resolution")) c->menu_res = percent_to_res(val);
        else if (!strcmp(name, "gameplay_resolution")) c->game_res = percent_to_res(val);
        else if (!strcmp(name, "cpu_boost")) c->boost = val;
        else if (!strcmp(name, "particles")) c->particles = val;
        else if (!strcmp(name, "live_screens")) c->live_screens = val;
        else if (!strcmp(name, "vsync")) c->vsync = val;
        else if (!strcmp(name, "show_fps")) c->show_fps = val;
        else if (!strcmp(name, "frame_rate"))
            c->frame_rate = val > 0 && val <= 30 ? FRAME_RATE_30 : FRAME_RATE_60;
        else if (!strcmp(name, "lock_30_big_matches")) c->heavy30 = val;
    }
    fclose(f);
    clamp_settings(c);
    gfx_settings_resolve(&g_gfx, g_gfx_preset, c);
    if (g_gfx_preset == GFX_PRESET_CUSTOM) g_gfx_preset = gfx_settings_match(&g_gfx);
}

bool gfx_settings_save(void)
{
    const GfxSettings* c = &g_gfx_custom;
    FILE* f;
    bool ok;

    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/melee", 0777);
    f = fopen(GFX_TMP_PATH, "w");
    if (!f) return false;
    fprintf(f, "# Melee Vita graphics settings\n");
    fprintf(f, "# Change these from the Settings button on the LiveArea screen.\n");
    fprintf(f, "# preset: performance, quality or custom. The values below are\n");
    fprintf(f, "# the custom ones; the other two presets ignore them, apart from\n");
    fprintf(f, "# lock_30_big_matches and show_fps, which apply to every preset.\n");
    fprintf(f, "preset=%s\n", g_gfx_preset == GFX_PRESET_PERFORMANCE ? "performance"
                             : g_gfx_preset == GFX_PRESET_QUALITY ? "quality" : "custom");
    fprintf(f, "menu_resolution=%d\n", res_to_percent(c->menu_res));
    fprintf(f, "gameplay_resolution=%d\n", res_to_percent(c->game_res));
    fprintf(f, "cpu_boost=%d\n", c->boost);
    fprintf(f, "particles=%d\n", c->particles);
    fprintf(f, "live_screens=%d\n", c->live_screens);
    fprintf(f, "vsync=%d\n", c->vsync);
    fprintf(f, "show_fps=%d\n", c->show_fps);
    fprintf(f, "frame_rate=%d\n", c->frame_rate == FRAME_RATE_30 ? 30 : 60);
    fprintf(f, "# 30 fps for matches with 3-4 players or on Fountain of Dreams\n");
    fprintf(f, "lock_30_big_matches=%d\n", c->heavy30);
    ok = fclose(f) == 0;
    if (ok) {
        remove(GFX_PATH);
        ok = rename(GFX_TMP_PATH, GFX_PATH) == 0;
    }
    if (!ok) {
        remove(GFX_TMP_PATH);
        melee_vita_log_info("[GFX] failed to save " GFX_PATH);
    }
    return ok;
}

int g_melee_vita_fps30_active;

static void set_fps30(int on)
{
    g_melee_vita_fps30_active = on;
    g_melee_vita_min_ticks = on ? 2 : 1;
}

// decided once when the match starts, so the frame rate never changes mid-match
void melee_vita_match_begin(int players, int stage_kind)
{
    if (!g_gfx.heavy30 || g_gfx.frame_rate == FRAME_RATE_30) return;
    set_fps30(players >= 3 || stage_kind == 0x02 /* Fountain of Dreams */);
    melee_vita_log_info("[GFX] match: %d players, stage 0x%02x -> %s fps", players, stage_kind,
                        g_melee_vita_fps30_active ? "30" : "60");
}

void melee_vita_match_end(void)
{
    set_fps30(g_gfx.frame_rate == FRAME_RATE_30); // menus go back to the setting
}

void gfx_settings_apply(void)
{
    g_melee_vita_particle_limit = (u32) g_gfx.particles;
    g_melee_vita_copy_interval = (u32) g_gfx.live_screens;
    set_fps30(g_gfx.frame_rate == FRAME_RATE_30);

    // off = leave whatever the system gave us
    if (g_gfx.boost) {
        int a = scePowerSetArmClockFrequency(444);
        int b = scePowerSetBusClockFrequency(222);
        int g = scePowerSetGpuClockFrequency(222);
        int x = scePowerSetGpuXbarClockFrequency(166);
        melee_vita_log_info("[GFX] clock boost results %08x %08x %08x %08x",
                            (u32) a, (u32) b, (u32) g, (u32) x);
    }
    melee_vita_log_info("[GFX] clocks arm=%d bus=%d gpu=%d xbar=%d particles=%d live_screens=%d",
                        scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(),
                        scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency(),
                        g_gfx.particles, g_gfx.live_screens);
}

/* ---- fps counter ---- */

extern u32 g_melee_vita_tick_total; // gmscene.c

static vita2d_pgf* font;
// written by the game thread once a second, read by the render thread
static volatile int fps_x10, game_x10, worst_ms_x10, arm_mhz, gpu_mhz;

// shared by the fps counter and the benchmark overlay, render thread only
vita2d_pgf* gfx_font(void)
{
    if (!font) {
        char ascii[96];
        font = vita2d_load_default_pgf();
        if (!font) return NULL;
        // put every glyph in the atlas now so it doesn't change mid-frame later
        for (int i = 0; i < 95; i++) ascii[i] = (char) (32 + i);
        ascii[95] = 0;
        vita2d_pgf_text_width(font, 1.0f, ascii);
    }
    return font;
}

void gfx_fps_tick(void)
{
    static u64 window_start, last;
    static u32 frames, start_ticks, worst_us;
    u64 now;

    if (!g_gfx.show_fps) return;
    now = sceKernelGetProcessTimeWide();
    if (window_start == 0) {
        window_start = last = now;
        start_ticks = g_melee_vita_tick_total;
        return;
    }
    if (now - last > worst_us) worst_us = (u32) (now - last);
    last = now;
    frames++;
    if (now - window_start >= 1000000u) {
        u64 span = now - window_start;
        fps_x10 = (int) (frames * 10000000ull / span);
        game_x10 = (int) ((g_melee_vita_tick_total - start_ticks) * 10000000ull / span);
        worst_ms_x10 = (int) (worst_us / 100u);
        arm_mhz = scePowerGetArmClockFrequency();
        gpu_mhz = scePowerGetGpuClockFrequency();
        window_start = now;
        start_ticks = g_melee_vita_tick_total;
        frames = 0;
        worst_us = 0;
    }
}

void gfx_fps_draw(void)
{
    char text[96];
    int f, g, w;

    if (!g_gfx.show_fps || !gfx_font()) return;
    f = fps_x10;
    g = game_x10;
    w = worst_ms_x10;
    snprintf(text, sizeof(text), "%d.%d FPS  game %d.%d/s  worst %d.%d ms  CPU %d GPU %d",
             f / 10, f % 10, g / 10, g % 10, w / 10, w % 10, arm_mhz, gpu_mhz);
    vita2d_draw_rectangle(4.0f, 4.0f, (float) vita2d_pgf_text_width(font, 0.8f, text) + 12.0f,
                          24.0f, RGBA8(0, 0, 0, 160));
    vita2d_pgf_draw_text(font, 10, 22, RGBA8(255, 255, 0, 255), 0.8f, text);
}
