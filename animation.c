/* ============================================================================
 * 文件: animation.c
 * 说明: 动画播放模块实现
 *      按编号顺序加载 BMP 并显示, 帧间按 frame_ms 延时.
 * ==========================================================================*/
#include "animation.h"
#include "ui.h"
#include "bmp_display.h"   /* bmp_info_t */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

/* 工具: 毫秒级睡眠 */
static void sleep_ms(int ms)
{
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* 构造完整路径: dir/prefix + 编号 + suffix */
static int make_path(char *out, size_t out_sz,
                     const char *dir, const char *prefix,
                     int idx, const char *suffix)
{
    int n = snprintf(out, out_sz, "%s/%s%03d%s", dir, prefix, idx, suffix);
    if (n < 0 || (size_t)n >= out_sz) return -1;
    return 0;
}

/* 探测目录中以 prefix 开头的最大编号, 用于 last_idx = -1 */
int animation_scan(anim_opts_t *opts)
{
    if (!opts || !opts->dir) return -1;

    DIR *d = opendir(opts->dir);
    if (!d) {
        fprintf(stderr, "[anim] 无法打开目录: %s (%s)\n",
                opts->dir, strerror(errno));
        return -1;
    }
    const char *prefix = opts->prefix ? opts->prefix : "frame";
    size_t plen = strlen(prefix);
    int max_idx = -1, count = 0;

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strncmp(de->d_name, prefix, plen) != 0) continue;
        const char *num = de->d_name + plen;
        char *end = NULL;
        long n = strtol(num, &end, 10);
        if (end == num || n < 0) continue;
        if ((int)n > max_idx) max_idx = (int)n;
        count++;
    }
    closedir(d);

    if (opts->last_idx < 0) opts->last_idx = max_idx;
    if (count == 0) return -1;
    return count;
}

int animation_play(const anim_opts_t *opts, volatile int *interrupt)
{
    if (!opts || !opts->dir) return -1;

    /* 拷贝 opts, 避免修改 const 入参 */
    anim_opts_t o = *opts;
    if (!o.prefix) o.prefix = "frame";
    if (!o.suffix) o.suffix = ".bmp";
    if (o.frame_ms <= 0) o.frame_ms = 50;
    if (o.first_idx < 0) o.first_idx = 0;

    if (o.last_idx < 0) {
        if (animation_scan(&o) < 0) {
            fprintf(stderr, "[anim] 目录 %s 中未找到帧文件 (前缀 %s)\n",
                    o.dir, o.prefix);
            return -1;
        }
    }
    if (o.last_idx < o.first_idx) {
        fprintf(stderr, "[anim] 帧范围无效: [%d..%d]\n",
                o.first_idx, o.last_idx);
        return -1;
    }

    printf("[anim] 播放目录: %s  帧数: %d  帧时: %dms  loop=%d\n",
           o.dir, o.last_idx - o.first_idx + 1, o.frame_ms, o.loop);

    char path[512];
    for (;;) {
        for (int i = o.first_idx; i <= o.last_idx; i++) {
            if (interrupt && *interrupt) return 1;
            if (make_path(path, sizeof(path), o.dir, o.prefix, i, o.suffix) < 0)
                continue;

            if (o.fill_bg) ui_fill_rect(0, 0, g_screen_w, g_screen_h, o.bg_color);
            /* 等比缩放到屏幕并居中绘制, 避免大图超出屏幕 */
            bmp_info_t bmp;
            double scale = 1.0;
            int img_x = 0, img_y = 0;
            if (read_bmp_info(path, &bmp) >= 0 &&
                bmp.width > 0 && bmp.height > 0) {
                double sw = (double)g_screen_w / (double)bmp.width;
                double sh = (double)g_screen_h / (double)bmp.height;
                scale = (sw < sh) ? sw : sh;
                if (scale > 1.0) scale = 1.0;       /* 不放大 */
                if (scale < 0.01) scale = 0.01;     /* 避免为 0 */
                unsigned int dw = (unsigned int)(bmp.width  * scale);
                unsigned int dh = (unsigned int)(bmp.height * scale);
                if (dw == 0) dw = 1;
                if (dh == 0) dh = 1;
                img_x = ((int)g_screen_w - (int)dw) / 2;
                img_y = ((int)g_screen_h - (int)dh) / 2;
            } else {
                /* 读不到头信息: 仍尝试按原尺寸画, 让 ui_draw_bmp 报错 */
                scale = 0.0;
            }
            ui_draw_bmp(path, img_x, img_y, scale);

            sleep_ms(o.frame_ms);
        }
        if (!o.loop) return 0;
    }
}
