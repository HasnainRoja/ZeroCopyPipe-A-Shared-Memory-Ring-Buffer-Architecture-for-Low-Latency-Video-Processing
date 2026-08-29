#include "../include/pipeline.h"

static uint8_t frame_buf_a[FRAME_SIZE];
static uint8_t frame_buf_b[FRAME_SIZE];
static uint8_t frame_buf_c[FRAME_SIZE];
static uint8_t frame_buf_d[FRAME_SIZE];

static void bl_generate(uint8_t *px, uint64_t fid) {
    int cx = FRAME_WIDTH / 2, cy = FRAME_HEIGHT / 2, r = 300;
    int ix = (int)(fid * 3) % FRAME_WIDTH;
    memset(px, 10, FRAME_SIZE);
    for (int y = 0; y < FRAME_HEIGHT; y += 2) {
        for (int x = 0; x < FRAME_WIDTH; x += 2) {
            int i = (y * FRAME_WIDTH + x) * 3;
            int dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy < r * r) {
                px[i]     = (uint8_t)(170 + (fid % 40));
                px[i + 1] = (uint8_t)(80 + (x % 35));
                px[i + 2] = (uint8_t)(70 + (y % 25));
            }
            if (abs(x - ix) < 8) {
                px[i] = 230; px[i + 1] = 230; px[i + 2] = 210;
            }
        }
    }
}

static void bl_denoise(uint8_t *out, const uint8_t *in) {
    memcpy(out, in, FRAME_SIZE);
    for (int y = 1; y < FRAME_HEIGHT - 1; y += 8) {
        for (int x = 1; x < FRAME_WIDTH - 1; x += 8) {
            int i = (y * FRAME_WIDTH + x) * 3;
            int a = ((y - 1) * FRAME_WIDTH + x) * 3;
            int b = ((y + 1) * FRAME_WIDTH + x) * 3;
            int l = (y * FRAME_WIDTH + (x - 1)) * 3;
            int r = (y * FRAME_WIDTH + (x + 1)) * 3;
            for (int c = 0; c < 3; c++)
                out[i + c] = (uint8_t)((in[i + c] + in[a + c] + in[b + c] +
                                        in[l + c] + in[r + c]) / 5);
        }
    }
}

static void bl_edge(uint8_t *out, const uint8_t *in) {
    memcpy(out, in, FRAME_SIZE);
    for (int y = 1; y < FRAME_HEIGHT - 1; y += 8) {
        for (int x = 1; x < FRAME_WIDTH - 1; x += 8) {
            int i  = (y * FRAME_WIDTH + x) * 3;
            int ri = (y * FRAME_WIDTH + (x + 1)) * 3;
            int bi = ((y + 1) * FRAME_WIDTH + x) * 3;
            int mag = abs((int)in[ri] - (int)in[i]) + abs((int)in[bi] - (int)in[i]);
            if (mag > 30) {
                out[i]     = (uint8_t)(in[i] + 50 > 255 ? 255 : in[i] + 50);
                out[i + 1] = (uint8_t)(in[i + 1] + 80 > 255 ? 255 : in[i + 1] + 80);
                out[i + 2] = (uint8_t)(in[i + 2] + 80 > 255 ? 255 : in[i + 2] + 80);
            }
        }
    }
}

static void bl_overlay(uint8_t *out, const uint8_t *in, uint64_t fid) {
    memcpy(out, in, FRAME_SIZE);
    int cx = FRAME_WIDTH / 2, cy = FRAME_HEIGHT / 2;
    int ir = 220, or2 = 235;
    for (int y = cy - or2; y <= cy + or2; y += 2) {
        if (y < 0 || y >= FRAME_HEIGHT) continue;
        for (int x = cx - or2; x <= cx + or2; x += 2) {
            if (x < 0 || x >= FRAME_WIDTH) continue;
            int dx = x - cx, dy = y - cy, d2 = dx * dx + dy * dy;
            if (d2 >= ir * ir && d2 <= or2 * or2) {
                int i = (y * FRAME_WIDTH + x) * 3;
                out[i] = 255; out[i + 1] = 50; out[i + 2] = 50;
            }
        }
    }
    int ix = (int)(fid * 3) % FRAME_WIDTH;
    for (int y = cy - 20; y <= cy + 20; y++) {
        if (y < 0 || y >= FRAME_HEIGHT) continue;
        int i = (y * FRAME_WIDTH + ix) * 3;
        out[i] = 255; out[i + 1] = 255; out[i + 2] = 0;
    }
}

void run_baseline(void) {
    printf(COLOR_BOLD COLOR_RED
           "\n[BASELINE] copy-based pipeline (%d frames)\n" COLOR_RESET,
           BENCHMARK_FRAMES);

    FILE *log = fopen("../results/baseline_log.csv", "w");
    if (!log) log = fopen("results/baseline_log.csv", "w");
    if (!log) {
        perror("fopen baseline_log.csv");
        return;
    }

    fprintf(log,
            "frame_id,write_ns,after_denoise_ns,after_edge_ns,"
            "after_overlay_ns,display_ns,end_to_end_us\n");

    uint64_t total = 0, max_ns = 0, min_ns = UINT64_MAX;

    for (uint64_t fid = 0; fid < (uint64_t)BENCHMARK_FRAMES; fid++) {
        uint64_t t0 = now_ns();
        bl_generate(frame_buf_a, fid);
        bl_denoise(frame_buf_b, frame_buf_a);
        uint64_t t1 = now_ns();
        bl_edge(frame_buf_c, frame_buf_b);
        uint64_t t2 = now_ns();
        bl_overlay(frame_buf_d, frame_buf_c, fid);
        uint64_t t3 = now_ns();
        uint64_t t4 = now_ns();

        uint64_t e2e = t4 - t0;
        total += e2e;
        if (e2e > max_ns) max_ns = e2e;
        if (e2e < min_ns) min_ns = e2e;

        fprintf(log, "%lu,%lu,%lu,%lu,%lu,%lu,%.3f\n",
                (unsigned long)fid,
                (unsigned long)t0, (unsigned long)t1,
                (unsigned long)t2, (unsigned long)t3,
                (unsigned long)t4, e2e / 1000.0);

        if (fid % 30 == 0) {
            printf(COLOR_RED "[BASELINE] frame %lu | e2e=%.2f ms\n" COLOR_RESET,
                   (unsigned long)fid, e2e / 1e6);
        }
    }

    fclose(log);
    printf(COLOR_BOLD COLOR_RED
           "[BASELINE] avg=%.3f ms min=%.3f max=%.3f\n" COLOR_RESET,
           (total / BENCHMARK_FRAMES) / 1e6, min_ns / 1e6, max_ns / 1e6);
}