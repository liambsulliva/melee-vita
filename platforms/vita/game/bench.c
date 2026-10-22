/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "bench.h"
#include "gfx_settings.h"
#include "gxm_resolution.h"
#include "../vita_log.h"

#include <psp2/ctrl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>
#include <vita2d.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BENCH_DIR "ux0:data/melee/benchmark"
#define BENCH_REQUEST BENCH_DIR "/request"

// in game ticks (60 per second of game time)
#define WARMUP_TICKS (5 * 60)
#define MEASURE_TICKS (60 * 60)
#define MAX_ROWS 8000 // room for 60 s at 60 fps plus slowdown

// StKind values (src/melee/gr/forward.h)
static const struct {
    int kind;
    const char* name;
} stages[] = {
    { 0x1F, "Battlefield" },
    { 0x20, "Final Destination" },
    { 0x08, "Yoshi's Story" },
    { 0x03, "Pokemon Stadium" },
    { 0x02, "Fountain of Dreams" },
    { 0x1C, "Dream Land N64" },
};
#define STAGE_COUNT ((int) (sizeof(stages) / sizeof(stages[0])))

typedef struct {
    u32 dt_us;     // time since the previous frame was handed to the GPU
    u32 ticks;     // game logic ticks run for this frame (1 = full speed)
    u32 update_us; // game logic
    u32 render_us; // building the frame on the CPU, includes wait_us
    u32 wait_us;   // waiting for the render thread to finish the last frame
} Row;

typedef struct {
    int frames;
    float avg_fps, low1_fps, low01_fps, min_fps;
    float avg_ms, stdev_ms, worst_ms, jitter_ms;
    float over17, over25, over34; // % of frames
    float game_speed;             // logic ticks per second, 60 = full speed
    float update_ms, render_ms, wait_ms;
    int hist[6];
} Stats;

static const char* hist_names[6] = { "<17.5", "17.5-20", "20-25", "25-34", "34-50", ">50" };

extern u32 g_melee_vita_tick_total;       // gmscene.c
extern u64 g_melee_vita_update_total_us;  // gmscene.c
extern u64 g_melee_vita_render_total_us;  // gmscene.c

static int active;
static int stage_i;
static int recording;
static int warmed_up; // recording started for this stage
static int cancelled;
static int done;      // finished (or cancelled), results on screen
static u64 done_at;
static u32 cur_frame;
static int report_num;

static Row rows[MAX_ROWS];
static int nrows;
static u64 last_present;
static u32 last_ticks;
static u64 last_update, last_render;

static Stats results[STAGE_COUNT];
static int nresults;

/* ---- stats ---- */

static int cmp_desc(const void* a, const void* b)
{
    u32 x = *(const u32*) a, y = *(const u32*) b;
    return x < y ? 1 : x > y ? -1 : 0;
}

// average fps over the slowest `frac` of frames
static float low_fps(const u32* sorted, int n, float frac)
{
    int k = (int) (n * frac);
    double sum = 0;
    if (k < 1) k = 1;
    for (int i = 0; i < k; i++) sum += sorted[i];
    return sum > 0 ? (float) (1000000.0 * k / sum) : 0.0f;
}

static void compute_stats(const Row* r, int n, Stats* s)
{
    static u32 sorted[MAX_ROWS];
    double total = 0, sq = 0, jit = 0, upd = 0, ren = 0, wai = 0;
    u32 ticks = 0;
    int o17 = 0, o25 = 0, o34 = 0;

    memset(s, 0, sizeof(*s));
    s->frames = n;
    if (n < 2) return;

    for (int i = 0; i < n; i++) {
        double ms = r[i].dt_us / 1000.0;
        total += ms;
        sq += ms * ms;
        if (i > 0) jit += fabs(ms - r[i - 1].dt_us / 1000.0);
        ticks += r[i].ticks;
        upd += r[i].update_us;
        ren += r[i].render_us;
        wai += r[i].wait_us;
        if (ms > 17.5) o17++;
        if (ms > 25.0) o25++;
        if (ms > 34.0) o34++;

        if (ms <= 17.5) s->hist[0]++;
        else if (ms <= 20) s->hist[1]++;
        else if (ms <= 25) s->hist[2]++;
        else if (ms <= 34) s->hist[3]++;
        else if (ms <= 50) s->hist[4]++;
        else s->hist[5]++;

        sorted[i] = r[i].dt_us;
    }
    qsort(sorted, n, sizeof(u32), cmp_desc);

    s->avg_ms = (float) (total / n);
    s->avg_fps = (float) (1000.0 * n / total);
    s->stdev_ms = (float) sqrt(fmax(0.0, sq / n - (total / n) * (total / n)));
    s->jitter_ms = (float) (jit / (n - 1));
    s->worst_ms = sorted[0] / 1000.0f;
    s->min_fps = sorted[0] ? 1000000.0f / sorted[0] : 0.0f;
    s->low1_fps = low_fps(sorted, n, 0.01f);
    s->low01_fps = low_fps(sorted, n, 0.001f);
    s->over17 = 100.0f * o17 / n;
    s->over25 = 100.0f * o25 / n;
    s->over34 = 100.0f * o34 / n;
    s->game_speed = (float) (ticks * 1000.0 / total);
    s->update_ms = (float) (upd / n / 1000.0);
    s->render_ms = (float) (fmax(0.0, ren - wai) / n / 1000.0); // without the wait
    s->wait_ms = (float) (wai / n / 1000.0);
}

/* ---- files ---- */

static void report_path(char* out, int size, const char* ext)
{
    snprintf(out, size, BENCH_DIR "/bench-%03d.%s", report_num, ext);
}

static void write_csv_rows(void)
{
    char path[96];
    FILE* f;
    report_path(path, sizeof(path), "csv");
    f = fopen(path, nresults == 0 ? "w" : "a");
    if (!f) return;
    if (nresults == 0)
        fprintf(f, "stage,frame,frame_ms,ticks,logic_ms,draw_ms,wait_ms\n");
    for (int i = 0; i < nrows; i++) {
        const Row* r = &rows[i];
        u32 draw = r->render_us > r->wait_us ? r->render_us - r->wait_us : 0;
        fprintf(f, "%s,%d,%.2f,%u,%.2f,%.2f,%.2f\n", stages[stage_i].name, i,
                r->dt_us / 1000.0, (unsigned) r->ticks, r->update_us / 1000.0,
                draw / 1000.0, r->wait_us / 1000.0);
    }
    fclose(f);
}

// rewritten after every stage so a crash still leaves the stages so far
static void write_summary(void)
{
    char path[96];
    FILE* f;
    report_path(path, sizeof(path), "txt");
    f = fopen(path, "w");
    if (!f) return;

    fprintf(f, "Melee Vita benchmark %03d%s\n\n", report_num,
            cancelled ? " (cancelled)" : done ? "" : " (incomplete)");
    fprintf(f, "Settings: preset %s, menu %s, gameplay %s, boost %s, particles %d, "
               "live screens every %d, vsync %s\n",
            gfx_settings_preset_name(gfx_settings_match_preset(&g_gfx)),
            g_gfx.menu_res == 0 ? "100%" : g_gfx.menu_res == 1 ? "75%" : g_gfx.menu_res == 2 ? "60%" : "50%",
            g_gfx.game_res == 0 ? "100%" : g_gfx.game_res == 1 ? "75%" : g_gfx.game_res == 2 ? "60%" : "50%",
            g_gfx.boost ? "on" : "off", g_gfx.particles, g_gfx.live_screens,
            g_gfx.vsync ? "on" : "off");
    fprintf(f, "Clocks: CPU %d MHz, bus %d MHz, GPU %d MHz, crossbar %d MHz\n",
            scePowerGetArmClockFrequency(), scePowerGetBusClockFrequency(),
            scePowerGetGpuClockFrequency(), scePowerGetGpuXbarClockFrequency());
    fprintf(f, "Match: Fox, Falco, Marth, Captain Falcon (all CPU level 9), items off.\n");
    fprintf(f, "Each stage: %d s warm-up, then %d s of game time measured.\n\n",
            WARMUP_TICKS / 60, MEASURE_TICKS / 60);

    fprintf(f, "%-19s %7s %7s %8s %7s %7s %8s %8s %9s %7s %7s %7s %6s\n", "stage", "avg fps",
            "1% low", "0.1% low", "min fps", "avg ms", "stdev ms", "worst ms", "jitter ms",
            ">17.5ms", ">25ms", ">34ms", "speed");
    for (int i = 0; i < nresults; i++) {
        const Stats* s = &results[i];
        fprintf(f, "%-19s %7.1f %7.1f %8.1f %7.1f %7.2f %8.2f %8.1f %9.2f %6.1f%% %6.1f%% %6.1f%% %5.1f%%\n",
                stages[i].name, s->avg_fps, s->low1_fps, s->low01_fps, s->min_fps, s->avg_ms,
                s->stdev_ms, s->worst_ms, s->jitter_ms, s->over17, s->over25, s->over34,
                100.0f * s->game_speed / 60.0f);
    }

    fprintf(f, "\nFrame time histogram (number of frames, ms)\n%-19s", "stage");
    for (int b = 0; b < 6; b++) fprintf(f, " %8s", hist_names[b]);
    fprintf(f, "\n");
    for (int i = 0; i < nresults; i++) {
        fprintf(f, "%-19s", stages[i].name);
        for (int b = 0; b < 6; b++) fprintf(f, " %8d", results[i].hist[b]);
        fprintf(f, "\n");
    }

    fprintf(f, "\nGame thread time per frame (ms)\n%-19s %8s %8s %8s\n", "stage", "logic",
            "draw", "wait");
    for (int i = 0; i < nresults; i++)
        fprintf(f, "%-19s %8.2f %8.2f %8.2f\n", stages[i].name, results[i].update_ms,
                results[i].render_ms, results[i].wait_ms);

    fprintf(f, "\nNotes:\n"
               "- 1%% / 0.1%% low: average fps of the slowest 1%% / 0.1%% of frames.\n"
               "- jitter: average change in frame time from one frame to the next.\n"
               "  High jitter feels choppy even when the average fps is fine.\n"
               "- speed: game speed, 100%% means the game logic kept up with real time.\n"
               "- wait: time the game thread waited for the render thread (GPU side)\n"
               "  to finish the previous frame. High wait means GPU-bound, high logic\n"
               "  or draw means CPU-bound.\n"
               "- Per-frame data is in bench-%03d.csv.\n",
            report_num);
    fclose(f);
}

/* ---- request / setup ---- */

void melee_vita_bench_request(void)
{
    FILE* f;
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/melee", 0777);
    sceIoMkdir(BENCH_DIR, 0777);
    f = fopen(BENCH_REQUEST, "w");
    if (f) {
        fputs("run\n", f);
        fclose(f);
    }
}

void melee_vita_bench_init(void)
{
    FILE* f = fopen(BENCH_REQUEST, "r");
    char path[96];
    if (!f) return;
    fclose(f);
    remove(BENCH_REQUEST); // one run per request, a crash won't loop

    // next free report number
    for (report_num = 1; report_num < 1000; report_num++) {
        report_path(path, sizeof(path), "txt");
        f = fopen(path, "r");
        if (!f) break;
        fclose(f);
    }
    active = 1;
    stage_i = 0;
    melee_vita_log_info("[BENCH] starting benchmark %03d", report_num);
}

int melee_vita_bench_active(void)
{
    return active && !done;
}

int melee_vita_bench_stage(void)
{
    return stages[stage_i < STAGE_COUNT ? stage_i : 0].kind;
}

u32 melee_vita_bench_seed(void)
{
    return 0x4D454C45u + (u32) stage_i; // same match every run
}

/* ---- recording ---- */

static void finish_stage(void)
{
    compute_stats(rows, nrows, &results[nresults]);
    write_csv_rows();
    nresults++;
    melee_vita_log_info("[BENCH] %s: %.1f fps avg, %.1f 1%% low, %.1f ms worst",
                        stages[stage_i].name, results[nresults - 1].avg_fps,
                        results[nresults - 1].low1_fps, results[nresults - 1].worst_ms);
}

int melee_vita_bench_match_tick(u32 frame, int cancel)
{
    if (!melee_vita_bench_active()) return 0;
    cur_frame = frame;

    if (cancel) {
        recording = 0;
        if (nrows > 60) finish_stage();
        cancelled = 1;
        done = 1;
        done_at = sceKernelGetProcessTimeWide();
        write_summary();
        return 1;
    }
    if (frame >= WARMUP_TICKS && !warmed_up) {
        nrows = 0;
        last_present = 0;
        recording = 1;
        warmed_up = 1;
    }
    if (recording && frame >= WARMUP_TICKS + MEASURE_TICKS) {
        recording = 0;
        warmed_up = 0;
        finish_stage();
        stage_i++;
        if (stage_i >= STAGE_COUNT) {
            done = 1;
            done_at = sceKernelGetProcessTimeWide();
        }
        write_summary();
        return 1;
    }
    return 0;
}

void melee_vita_bench_frame(u32 sync_wait_us)
{
    u64 now, upd, ren;
    u32 ticks;

    if (!recording) return;
    now = sceKernelGetProcessTimeWide();
    ticks = g_melee_vita_tick_total;
    upd = g_melee_vita_update_total_us;
    ren = g_melee_vita_render_total_us;

    if (last_present != 0 && nrows < MAX_ROWS) {
        Row* r = &rows[nrows];
        r->dt_us = (u32) (now - last_present);
        r->ticks = ticks - last_ticks;
        r->update_us = (u32) (upd - last_update);
        r->wait_us = sync_wait_us;
        r->render_us = 0;
        // the render total only goes up after present returns, so it
        // belongs to the previous frame
        if (nrows > 0) rows[nrows - 1].render_us = (u32) (ren - last_render);
        nrows++;
    }
    last_present = now;
    last_ticks = ticks;
    last_update = upd;
    last_render = ren;
}

/* ---- overlay ---- */

int melee_vita_bench_wants_overlay(void)
{
    return active;
}

static void draw_results(vita2d_pgf* font)
{
    static const int cols[] = { 60, 300, 400, 500, 610, 720 };
    char text[96];
    int y = 120;
    u32 white = RGBA8(240, 240, 240, 255), grey = RGBA8(170, 170, 170, 255);

    vita2d_draw_rectangle(40, 60, 880, 420, RGBA8(0, 0, 0, 220));
    snprintf(text, sizeof(text), "Benchmark %03d %s", report_num, cancelled ? "(cancelled)" : "done");
    vita2d_pgf_draw_text(font, 60, 95, RGBA8(255, 210, 60, 255), 1.1f, text);

    vita2d_pgf_draw_text(font, cols[0], y, grey, 0.85f, "stage");
    vita2d_pgf_draw_text(font, cols[1], y, grey, 0.85f, "avg fps");
    vita2d_pgf_draw_text(font, cols[2], y, grey, 0.85f, "1% low");
    vita2d_pgf_draw_text(font, cols[3], y, grey, 0.85f, "worst ms");
    vita2d_pgf_draw_text(font, cols[4], y, grey, 0.85f, "jitter ms");
    vita2d_pgf_draw_text(font, cols[5], y, grey, 0.85f, "> 17.5 ms");
    for (int i = 0; i < nresults; i++) {
        const Stats* s = &results[i];
        y += 34;
        vita2d_pgf_draw_text(font, cols[0], y, white, 0.9f, stages[i].name);
        snprintf(text, sizeof(text), "%.1f", s->avg_fps);
        vita2d_pgf_draw_text(font, cols[1], y, white, 0.9f, text);
        snprintf(text, sizeof(text), "%.1f", s->low1_fps);
        vita2d_pgf_draw_text(font, cols[2], y, white, 0.9f, text);
        snprintf(text, sizeof(text), "%.1f", s->worst_ms);
        vita2d_pgf_draw_text(font, cols[3], y, white, 0.9f, text);
        snprintf(text, sizeof(text), "%.2f", s->jitter_ms);
        vita2d_pgf_draw_text(font, cols[4], y, white, 0.9f, text);
        snprintf(text, sizeof(text), "%.1f%%", s->over17);
        vita2d_pgf_draw_text(font, cols[5], y, white, 0.9f, text);
    }
    snprintf(text, sizeof(text), "Full report: " BENCH_DIR "/bench-%03d.txt (and .csv)", report_num);
    vita2d_pgf_draw_text(font, 60, 430, grey, 0.8f, text);
    vita2d_pgf_draw_text(font, 60, 460, grey, 0.8f, "Press any button to close");
}

void melee_vita_bench_draw(void)
{
    vita2d_pgf* font = gfx_font();
    char text[128];

    if (!active || !font) return;

    if (done) {
        SceCtrlData pad;
        draw_results(font);
        // close after a button press, but not before it was up for 3 s
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons && sceKernelGetProcessTimeWide() - done_at > 3000000u)
            active = 0;
        return;
    }

    if (recording) {
        int left = (int) (WARMUP_TICKS + MEASURE_TICKS - (int) cur_frame) / 60;
        snprintf(text, sizeof(text), "BENCHMARK %d/%d  %s  %d s left   START: cancel", stage_i + 1,
                 STAGE_COUNT, stages[stage_i].name, left < 0 ? 0 : left);
    } else {
        snprintf(text, sizeof(text), "BENCHMARK %d/%d  %s  warming up   START: cancel",
                 stage_i + 1, STAGE_COUNT, stages[stage_i].name);
    }
    vita2d_draw_rectangle(4.0f, 516.0f, (float) vita2d_pgf_text_width(font, 0.8f, text) + 12.0f,
                          24.0f, RGBA8(0, 0, 0, 160));
    vita2d_pgf_draw_text(font, 10, 534, RGBA8(255, 210, 60, 255), 0.8f, text);
}
