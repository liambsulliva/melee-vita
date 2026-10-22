/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Graphics settings screen. The Settings button on the LiveArea starts the
 * game with "-settings" (livearea/template.xml) and main() shows this instead
 * of booting Melee. Same idea as mkwiita's Settings screen. */
#include "gfx_settings.h"
#include "gxm_resolution.h"
#include "bench.h"

#include <dolphin/types.h>
#include <psp2/appmgr.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <vita2d.h>

#include <stdio.h>
#include <string.h>

enum {
    ROW_PRESET,
    ROW_MENU_RES,
    ROW_GAME_RES,
    ROW_FRAMERATE,
    ROW_BOOST,
    ROW_PARTICLES,
    ROW_LIVE,
    ROW_VSYNC,
    ROW_HEAVY30, // from here down: not part of a preset
    ROW_FPS,
    ROW_BENCH,
    ROW_RESET,
    ROW_START,
    ROW_EXIT,
    ROW_COUNT,
};

static const int particle_steps[] = { 128, 256, 512, 1024, 2048 };
static const int live_steps[] = { 1, 2, 4, 8 };
#define COUNT(a) ((int) (sizeof(a) / sizeof((a)[0])))

static const char* help_text[ROW_COUNT] = {
    "Performance: 60 FPS (30 with 3-4 players). Quality: sharpest, 30 FPS. Edits make it Custom.",
    "Render size for menus. Lower runs faster, higher looks sharper.",
    "Render size during matches. Lower runs faster, higher looks sharper.",
    "30 is locked and even. The game itself always runs at full speed.",
    "Runs the CPU at 444 MHz and the GPU at 222 MHz. Uses more battery.",
    "Most particles drawn at once. Lower helps in big explosions.",
    "How often screens inside stages update (Pokemon Stadium, etc).",
    "Wait for the display refresh before showing each frame.",
    "Matches with 3-4 players or on Fountain of Dreams run at a locked, even 30 FPS.",
    "Shows FPS, worst frame time and clocks in the top left corner.",
    "Left/Right: 1v1 or 4 players. X: save, play 5 stages (~6 min), write a report.",
    "Go back to the Performance preset.",
    "Save, then start the game.",
    "Save, then close the game.",
};

bool settings_app_requested(void)
{
    char param[1024];
    memset(param, 0, sizeof(param));
    if (sceAppMgrGetAppParam(param) < 0) return false;
    return strstr(param, "settings") != NULL;
}

// step through a list of allowed values, snapping odd values from a hand
// edited file to the closest one
static int step_list(const int* list, int n, int cur, int dir)
{
    int i = 0;
    while (i < n - 1 && list[i] < cur) i++;
    if (list[i] != cur && dir > 0) i--; // cur was between two steps
    i += dir;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return list[i];
}

static int bench_players = 2;

static void change(GfxSettings* s, int row, int dir)
{
    switch (row) {
    case ROW_PRESET:
        gfx_settings_select((g_gfx_preset + dir + GFX_PRESET_COUNT) % GFX_PRESET_COUNT);
        return;
    // resolution options go 0 = native .. 3 = 50%, so "right" means smaller
    // number = sharper
    case ROW_MENU_RES:
        s->menu_res -= dir;
        if (s->menu_res < 0) s->menu_res = 0;
        if (s->menu_res >= MELEE_VITA_RESOLUTION_OPTION_COUNT)
            s->menu_res = MELEE_VITA_RESOLUTION_OPTION_COUNT - 1;
        break;
    case ROW_GAME_RES:
        s->game_res -= dir;
        if (s->game_res < 0) s->game_res = 0;
        if (s->game_res >= MELEE_VITA_RESOLUTION_OPTION_COUNT)
            s->game_res = MELEE_VITA_RESOLUTION_OPTION_COUNT - 1;
        break;
    case ROW_FRAMERATE: s->frame_rate = s->frame_rate == FRAME_RATE_30 ? FRAME_RATE_60 : FRAME_RATE_30; break;
    case ROW_BOOST: s->boost = !s->boost; break;
    case ROW_PARTICLES:
        s->particles = step_list(particle_steps, COUNT(particle_steps), s->particles, dir);
        break;
    case ROW_LIVE:
        // right = updates more often = lower number
        s->live_screens = step_list(live_steps, COUNT(live_steps), s->live_screens, -dir);
        break;
    case ROW_VSYNC: s->vsync = !s->vsync; break;
    case ROW_HEAVY30: s->heavy30 = !s->heavy30; break;
    case ROW_FPS: s->show_fps = !s->show_fps; break;
    case ROW_BENCH: bench_players = bench_players == 4 ? 2 : 4; return;
    default: return;
    }
    gfx_settings_changed(); // anything off the preset's values makes it Custom
}

static void value_text(const GfxSettings* s, int row, char* out, int size)
{
    u32 w = 0, h = 0;
    out[0] = 0;
    switch (row) {
    case ROW_PRESET:
        snprintf(out, size, "%s", gfx_settings_preset_name(g_gfx_preset));
        break;
    case ROW_MENU_RES:
    case ROW_GAME_RES: {
        int r = row == ROW_MENU_RES ? s->menu_res : s->game_res;
        static const int pct[] = { 100, 75, 60, 50 };
        melee_vita_resolution_option_dimensions(r, &w, &h);
        snprintf(out, size, "%d%%  (%ux%u)", pct[r], (unsigned) w, (unsigned) h);
        break;
    }
    case ROW_FRAMERATE:
        snprintf(out, size, "%s", s->frame_rate == FRAME_RATE_30 ? "30 (LOCKED)" : "60");
        break;
    case ROW_BOOST: snprintf(out, size, "%s", s->boost ? "ON" : "OFF"); break;
    case ROW_BENCH: snprintf(out, size, "%s", bench_players == 4 ? "4 PLAYERS" : "1 VS 1"); break;
    case ROW_PARTICLES: snprintf(out, size, "%d", s->particles); break;
    case ROW_LIVE:
        if (s->live_screens == 1) snprintf(out, size, "EVERY FRAME");
        else snprintf(out, size, "EVERY %d FRAMES", s->live_screens);
        break;
    case ROW_VSYNC: snprintf(out, size, "%s", s->vsync ? "ON" : "OFF"); break;
    case ROW_HEAVY30: snprintf(out, size, "%s", s->heavy30 ? "ON" : "OFF"); break;
    case ROW_FPS: snprintf(out, size, "%s", s->show_fps ? "ON" : "OFF"); break;
    default: break;
    }
}

static const char* row_names[ROW_COUNT] = {
    "PRESET", "MENU RESOLUTION", "GAMEPLAY RESOLUTION", "FRAME RATE", "CPU/GPU BOOST",
    "PARTICLES", "LIVE SCREENS", "V-SYNC", "30 FPS IN BIG MATCHES", "SHOW FPS", "RUN BENCHMARK",
    "RESET TO DEFAULT", "SAVE AND START", "SAVE AND EXIT",
};

/* Runs the settings screen. Never returns: it either relaunches the game
 * or exits. */
void settings_app_run(void)
{
    vita2d_pgf* font;
    SceCtrlData pad;
    u32 old_buttons = 0;
    int row = 0;
    int held = 0;
    int action = -1; // ROW_START / ROW_EXIT / ROW_BENCH / 0 for quit without saving

    gfx_settings_load();

    vita2d_init();
    vita2d_set_clear_color(RGBA8(16, 16, 24, 255));
    font = vita2d_load_default_pgf();
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    while (action < 0) {
        u32 pressed, buttons;
        int dir = 0;

        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);
        buttons = pad.buttons;
        // left stick works like the d-pad
        if (pad.ly < 40) buttons |= SCE_CTRL_UP;
        if (pad.ly > 215) buttons |= SCE_CTRL_DOWN;
        if (pad.lx < 40) buttons |= SCE_CTRL_LEFT;
        if (pad.lx > 215) buttons |= SCE_CTRL_RIGHT;
        pressed = buttons & ~old_buttons;

        // key repeat for held directions
        if (buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT)) {
            held++;
            if (held > 20 && held % 5 == 0)
                pressed |= buttons & (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | SCE_CTRL_RIGHT);
        } else {
            held = 0;
        }
        old_buttons = buttons;

        if (pressed & SCE_CTRL_UP) row = (row + ROW_COUNT - 1) % ROW_COUNT;
        if (pressed & SCE_CTRL_DOWN) row = (row + 1) % ROW_COUNT;
        if (pressed & SCE_CTRL_LEFT) dir = -1;
        if (pressed & SCE_CTRL_RIGHT) dir = 1;
        if ((pressed & SCE_CTRL_CROSS) && row < ROW_BENCH) dir = 1;
        // left/right on the benchmark row picks 1v1 / 4 players, X runs it
        if (dir) change(&g_gfx, row, dir);

        if (pressed & SCE_CTRL_CROSS) {
            if (row == ROW_RESET) {
                g_gfx.show_fps = 0;
                g_gfx.heavy30 = 1;
                gfx_settings_select(GFX_PRESET_PERFORMANCE);
            }
            if (row == ROW_START || row == ROW_EXIT || row == ROW_BENCH) action = row;
        }
        if (pressed & SCE_CTRL_CIRCLE) action = 0;

        vita2d_start_drawing();
        vita2d_clear_screen();
        if (font) {
            vita2d_pgf_draw_text(font, 60, 60, RGBA8(255, 210, 60, 255), 1.4f, "Graphics Settings");
            for (int i = 0; i < ROW_COUNT; i++) {
                char val[48];
                int y = 104 + i * 26;
                u32 col = i == row ? RGBA8(255, 210, 60, 255) : RGBA8(230, 230, 230, 255);
                if (i >= ROW_HEAVY30) y += 10; // gap before the options presets don't touch
                if (i >= ROW_BENCH) y += 10; // and before the buttons
                if (i == row)
                    vita2d_draw_rectangle(50, y - 20, 860, 25, RGBA8(60, 60, 90, 255));
                vita2d_pgf_draw_text(font, 70, y, col, 1.0f, row_names[i]);
                value_text(&g_gfx, i, val, sizeof(val));
                if (val[0]) {
                    // arrows around the value so it's clear left/right changes it
                    char tmp[64];
                    snprintf(tmp, sizeof(tmp), "<  %s  >", val);
                    vita2d_pgf_draw_text(font, 480, y, col, 1.0f, tmp);
                }
            }
            vita2d_pgf_draw_text(font, 60, 488, RGBA8(200, 200, 200, 255), 0.9f, help_text[row]);
            vita2d_pgf_draw_text(font, 60, 522, RGBA8(140, 140, 140, 255), 0.85f,
                                 "Up/Down: select   Left/Right or X: change   O: quit without saving");
        }
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    if (action != 0) gfx_settings_save();
    vita2d_wait_rendering_done();
    if (font) vita2d_free_pgf(font);
    vita2d_fini();

    if (action == ROW_BENCH) melee_vita_bench_request(bench_players);
    if (action == ROW_START || action == ROW_BENCH) {
        // start fresh without the -settings param
        sceAppMgrLoadExec("app0:eboot.bin", NULL, NULL);
    }
    sceKernelExitProcess(0);
}
