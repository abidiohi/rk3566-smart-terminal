/* ============================================================================
 * 文件: boot_anim.c
 * 说明: 开机/关机动画包装
 *      复用 animation 模块, 通过指定子目录区分开机/关机
 * ==========================================================================*/
#include "boot_anim.h"
#include "animation.h"
#include "input.h"
#include "ui.h"
#include "bmp_display.h"   /* bmp_info_t / read_bmp_* / scale_bmp_nearest */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

/* 输入轮询线程:
 *  - 开机动画允许触摸跳过, 需要一个独立线程监视触摸事件
 *  - 通过 g_skip 原子标记通知主循环停止
 */
static volatile int g_skip = 0;

static void *touch_watch_thread(void *arg)
{
    (void)arg;
    input_event_t ev;
    while (!g_skip) {
        int r = input_wait_event(&ev, 200);
        if (r == 0) {
            if (ev.type == INPUT_TAP ||
                ev.type == INPUT_SWIPE_LEFT ||
                ev.type == INPUT_SWIPE_RIGHT ||
                ev.type == INPUT_SWIPE_UP ||
                ev.type == INPUT_SWIPE_DOWN) {
                g_skip = 1;
                break;
            }
            if (ev.type == INPUT_QUIT) {
                g_skip = 1;
                break;
            }
        }
    }
    return NULL;
}

/* ---------- 工具函数 ---------- */

/* 毫秒级睡眠 */
static void sleep_ms(int ms)
{
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* 在 dir 下寻找一张 bmp.
 *  - 优先按 prefixNNN.bmp 顺序查找 (000..999)
 *  - 若都找不到, 扫描目录中第一个 *.bmp
 *  返回 0 找到并写入 out, -1 未找到
 */
static int find_single_bmp(const char *dir, const char *prefix,
                            char *out, size_t out_sz)
{
    /* 1) 按编号查找 */
    for (int i = 0; i < 1000; i++) {
        snprintf(out, out_sz, "%s/%s%03d.bmp", dir, prefix, i);
        if (access(out, R_OK) == 0) return 0;
    }
    /* 2) 任意 .bmp */
    DIR *d = opendir(dir);
    if (!d) return -1;
    struct dirent *de;
    int found = -1;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        size_t len = strlen(de->d_name);
        if (len <= 4) continue;
        const char *s = de->d_name + len - 4;
        if (s[0] == '.' &&
            (s[1] == 'b' || s[1] == 'B') &&
            (s[2] == 'm' || s[2] == 'M') &&
            (s[3] == 'p' || s[3] == 'P')) {
            snprintf(out, out_sz, "%s/%s", dir, de->d_name);
            found = 0;
            break;
        }
    }
    closedir(d);
    return found;
}

/* 居中等比缩放绘制一张图片, 并按 alpha 与白色做淡入混合.
 *  - alpha = 0.0 -> 纯白 (调用方需先 ui_clear(白))
 *  - alpha = 1.0 -> 原图全显
 *  - 中间值: out = src * alpha + 255 * (1 - alpha)
 *  返回 0 成功, -1 失败
 *
 * 备注: 假定非图像区域屏幕背景已为白 (由 boot_anim_play 启动时清屏保证)
 */
static int draw_bmp_white_fade(const char *path, double alpha)
{
    if (alpha < 0.0) alpha = 0.0;
    if (alpha > 1.0) alpha = 1.0;

    bmp_info_t info;
    if (read_bmp_info(path, &info) < 0) return -1;
    if (info.bpp != 24) {
        fprintf(stderr, "[boot] 仅支持 24 位 BMP (当前 %u 位)\n", info.bpp);
        return -1;
    }

    unsigned char *pixels = read_bmp_pixels(path, &info);
    if (!pixels) return -1;

    /* 等比缩放到屏幕 */
    double sw = (double)g_screen_w / (double)info.width;
    double sh = (double)g_screen_h / (double)info.height;
    double scale = (sw < sh) ? sw : sh;
    if (scale > 1.0) scale = 1.0;

    unsigned char *scaled = NULL;
    unsigned int dw = info.width, dh = info.height;
    unsigned int src_row_sz = info.row_size;
    if (scale < 0.999 || scale > 1.001) {
        scaled = scale_bmp_nearest(pixels, info.width, info.height,
                                   src_row_sz, scale, &dw, &dh);
        if (scaled) {
            src_row_sz = dw * 3;
        }
    }
    unsigned char *src = scaled ? scaled : pixels;

    int img_x = (g_screen_w - (int)dw) / 2;
    int img_y = (g_screen_h - (int)dh) / 2;

    int alpha_i     = (int)(alpha * 255.0);
    int inv_alpha_i = 255 - alpha_i;

    for (unsigned int dy = 0; dy < dh; dy++) {
        int sy = img_y + (int)dy;
        if (sy < 0 || sy >= g_screen_h) continue;
        int src_row;
        if (scaled) {
            src_row = info.bottom_up ? ((int)dh - 1 - (int)dy) : (int)dy;
        } else {
            src_row = info.bottom_up ? ((int)info.height - 1 - (int)dy) : (int)dy;
        }
        const unsigned char *row = src + src_row * src_row_sz;
        for (unsigned int dx = 0; dx < dw; dx++) {
            int sx = img_x + (int)dx;
            if (sx < 0 || sx >= g_screen_w) continue;
            unsigned int b = row[dx * 3 + 0];
            unsigned int g = row[dx * 3 + 1];
            unsigned int r = row[dx * 3 + 2];
            unsigned int out_r = (r * alpha_i + 255 * inv_alpha_i) / 255;
            unsigned int out_g = (g * alpha_i + 255 * inv_alpha_i) / 255;
            unsigned int out_b = (b * alpha_i + 255 * inv_alpha_i) / 255;
            unsigned int color = 0xFF000000u
                               | (out_r << 16)
                               | (out_g << 8)
                               |  out_b;
            ui_put_pixel(sx, sy, color);
        }
    }

    if (scaled) free(scaled);
    free(pixels);
    return 0;
}

/* ---------- 开机动画: 单张图 + 白屏淡入 ---------- */

int boot_anim_play(const char *root, int frame_ms, int skip_on_touch)
{
    (void)frame_ms;  /* 单张图淡入, 不再需要 frame_ms */

    const char *base = (root && root[0]) ? root : BOOT_ANIM_DEFAULT_ROOT;
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/boot", base);

    /* 寻找单张开机图 */
    char path[512];
    if (find_single_bmp(dir, "frame", path, sizeof(path)) < 0) {
        fprintf(stderr, "[boot] 开机图资源缺失, 显示文字版\n");
        ui_clear(0xFF101010u);
        const char *msg = "RK3566 Booting...";
        ui_draw_text_center(g_screen_w / 2, g_screen_h / 2,
                            msg, UI_COLOR_WHITE, 0xFF000000u, 4);
        sleep(2);
        return 0;
    }

    /* 启动触摸监听线程 (允许触摸跳过) */
    g_skip = 0;
    pthread_t th = 0;
    if (skip_on_touch) {
        if (pthread_create(&th, NULL, touch_watch_thread, NULL) != 0) {
            fprintf(stderr, "[boot] 创建输入监听线程失败, 继续播放\n");
        }
    }

    /* 白屏淡入: 50 步 × 80ms = 4000ms */
    const int FADE_STEPS = 50;
    const int FADE_MS    = 80;

    ui_clear(0xFFFFFFFFu);  /* 初始白屏 (同时作为非图像区域背景) */
    for (int step = 0; step <= FADE_STEPS; step++) {
        if (g_skip) break;
        double alpha = (double)step / (double)FADE_STEPS;
        if (draw_bmp_white_fade(path, alpha) < 0) {
            break;
        }
        sleep_ms(FADE_MS);
    }

    if (skip_on_touch && th) {
        g_skip = 1;
        pthread_join(th, NULL);
    }

    printf("[boot] 开机淡入完成 (%s)\n",
           g_skip ? "已跳过" : "完整播放");
    return 0;
}

/* ---------- 关机动画: 单张图 + 保持 5s ---------- */

int shutdown_anim_play(const char *root, int frame_ms)
{
    (void)frame_ms;  /* 不再使用 */

    const char *base = (root && root[0]) ? root : BOOT_ANIM_DEFAULT_ROOT;
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/shutdown", base);

    /* 寻找单张关机图 */
    char path[512];
    if (find_single_bmp(dir, "frame", path, sizeof(path)) < 0) {
        fprintf(stderr, "[shutdown] 关机图资源缺失\n");
        ui_clear(0xFF000000u);
        const char *msg = "Shutting down...";
        ui_draw_text_center(g_screen_w / 2, g_screen_h / 2,
                            msg, UI_COLOR_WHITE, 0xFF000000u, 4);
        sleep(1);
        return 0;
    }

    /* 加载图片信息并居中缩放 */
    bmp_info_t info;
    if (read_bmp_info(path, &info) < 0 || info.bpp != 24) {
        fprintf(stderr, "[shutdown] 读取关机图信息失败\n");
        return -1;
    }

    double sw = (double)g_screen_w / (double)info.width;
    double sh = (double)g_screen_h / (double)info.height;
    double scale = (sw < sh) ? sw : sh;
    if (scale > 1.0) scale = 1.0;

    unsigned int dw = (unsigned int)(info.width  * scale);
    unsigned int dh = (unsigned int)(info.height * scale);
    if (dw == 0) dw = 1;
    if (dh == 0) dh = 1;
    int img_x = (g_screen_w - (int)dw) / 2;
    int img_y = (g_screen_h - (int)dh) / 2;

    /* 一次性绘制 */
    ui_clear(0xFF000000u);
    if (ui_draw_bmp(path, img_x, img_y, scale) < 0) {
        fprintf(stderr, "[shutdown] 绘制关机图失败\n");
    }

    /* 保持显示 5 秒 (期间不响应触摸) */
    const int HOLD_MS = 5000;
    const int STEP_MS = 100;
    int elapsed = 0;
    while (elapsed < HOLD_MS) {
        sleep_ms(STEP_MS);
        elapsed += STEP_MS;
    }

    printf("[shutdown] 关机画面已显示 %dms\n", HOLD_MS);
    return 0;
}
