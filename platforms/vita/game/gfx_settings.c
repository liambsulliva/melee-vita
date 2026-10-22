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
#include <string.h>

#define GFX_PATH "ux0:data/melee/graphics.txt"
#define GFX_TMP_PATH "ux0:data/melee/graphics.tmp"
/* old file from the in-game Vita Options panel, only read once to migrate */
#define OLD_RES_PATH "ux0:data/melee/resolution-settings.bin"

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

GfxSettings g_gfx;

static const char* preset_names[] = { "PERFORMANCE", "DEFAULT", "QUALITY", "CUSTOM" };

/* TODO: Quality should lock 30 fps once the frame cap is in */
void gfx_settings_preset(GfxSettings* s, int preset)
{
    int fps = s->show_fps; // not part of a preset
    memset(s, 0, sizeof(*s));
    s->show_fps = fps;
    if (preset == GFX_PRESET_PERFORMANCE) {
        s->menu_res = MELEE_VITA_RESOLUTION_60;
        s->game_res = MELEE_VITA_RESOLUTION_60;
        s->boost = 1;
        s->particles = 256;
        s->live_screens = 8;
    } else if (preset == GFX_PRESET_QUALITY) {
        s->menu_res = MELEE_VITA_RESOLUTION_NATIVE;
        s->game_res = MELEE_VITA_RESOLUTION_NATIVE;
        s->boost = 1;
        s->particles = 2048;
        s->live_screens = 1;
    } else {
        // same as the release build before this menu existed
        s->menu_res = MELEE_VITA_DEFAULT_MENU_RESOLUTION_OPTION;
        s->game_res = MELEE_VITA_DEFAULT_GAMEPLAY_RESOLUTION_OPTION;
        s->boost = 0;
        s->particles = 512;
        s->live_screens = 4;
    }
}

int gfx_settings_match_preset(const GfxSettings* s)
{
    for (int i = 0; i < GFX_PRESET_COUNT; i++) {
        GfxSettings p = *s;
        gfx_settings_preset(&p, i);
        p.vsync = s->vsync; // vsync and fps counter don't count
        if (memcmp(&p, s, sizeof(p)) == 0)
            return i;
    }
    return GFX_PRESET_CUSTOM;
}

const char* gfx_settings_preset_name(int preset)
{
    if (preset < 0 || preset > GFX_PRESET_CUSTOM)
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
}

// pull the two resolution values out of resolution-settings.bin
static void migrate_old_file(void)
{
    struct { u32 magic, version; s32 menu, game; } old;
    FILE* f = fopen(OLD_RES_PATH, "rb");
    if (!f) return;
    if (fread(&old, sizeof(old), 1, f) == 1 && old.magic == 0x31525356u && old.version == 1) {
        g_gfx.menu_res = old.menu;
        g_gfx.game_res = old.game;
        melee_vita_log_info("[GFX] migrated resolution-settings.bin");
    }
    fclose(f);
}

void gfx_settings_load(void)
{
    char line[128];
    FILE* f;

    gfx_settings_preset(&g_gfx, GFX_PRESET_DEFAULT);
    g_gfx.show_fps = 0;

    f = fopen(GFX_PATH, "r");
    if (!f) {
        migrate_old_file();
        clamp_settings(&g_gfx);
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        char name[32];
        int val;
        if (line[0] == '#') continue;
        if (sscanf(line, " %31[^= ] = %d", name, &val) != 2) continue;

        if (!strcmp(name, "menu_resolution")) g_gfx.menu_res = percent_to_res(val);
        else if (!strcmp(name, "gameplay_resolution")) g_gfx.game_res = percent_to_res(val);
        else if (!strcmp(name, "cpu_boost")) g_gfx.boost = val;
        else if (!strcmp(name, "particles")) g_gfx.particles = val;
        else if (!strcmp(name, "live_screens")) g_gfx.live_screens = val;
        else if (!strcmp(name, "vsync")) g_gfx.vsync = val;
        else if (!strcmp(name, "show_fps")) g_gfx.show_fps = val;
    }
    fclose(f);
    clamp_settings(&g_gfx);
}

bool gfx_settings_save(void)
{
    FILE* f;
    bool ok;

    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/melee", 0777);
    f = fopen(GFX_TMP_PATH, "w");
    if (!f) return false;
    fprintf(f, "# Melee Vita graphics settings\n");
    fprintf(f, "# Change these from the Settings button on the LiveArea screen.\n");
    fprintf(f, "menu_resolution=%d\n", res_to_percent(g_gfx.menu_res));
    fprintf(f, "gameplay_resolution=%d\n", res_to_percent(g_gfx.game_res));
    fprintf(f, "cpu_boost=%d\n", g_gfx.boost);
    fprintf(f, "particles=%d\n", g_gfx.particles);
    fprintf(f, "live_screens=%d\n", g_gfx.live_screens);
    fprintf(f, "vsync=%d\n", g_gfx.vsync);
    fprintf(f, "show_fps=%d\n", g_gfx.show_fps);
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

void gfx_settings_apply(void)
{
    g_melee_vita_particle_limit = (u32) g_gfx.particles;
    g_melee_vita_copy_interval = (u32) g_gfx.live_screens;

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

static vita2d_pgf* fps_font;
// written by the game thread once a second, read by the render thread
static volatile int fps_x10, game_x10, worst_ms_x10, arm_mhz, gpu_mhz;

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

    if (!g_gfx.show_fps) return;
    if (!fps_font) {
        fps_font = vita2d_load_default_pgf();
        if (!fps_font) return;
        // put every glyph in the atlas now so it doesn't change mid-frame later
        vita2d_pgf_text_width(fps_font, 1.0f, "0123456789. FPSgameworstmsCUGPMHz/");
    }
    f = fps_x10;
    g = game_x10;
    w = worst_ms_x10;
    snprintf(text, sizeof(text), "%d.%d FPS  game %d.%d/s  worst %d.%d ms  CPU %d GPU %d",
             f / 10, f % 10, g / 10, g % 10, w / 10, w % 10, arm_mhz, gpu_mhz);
    vita2d_draw_rectangle(4.0f, 4.0f, (float) vita2d_pgf_text_width(fps_font, 0.8f, text) + 12.0f,
                          24.0f, RGBA8(0, 0, 0, 160));
    vita2d_pgf_draw_text(fps_font, 10, 22, RGBA8(255, 255, 0, 255), 0.8f, text);
}
