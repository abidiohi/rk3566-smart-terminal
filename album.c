/* ============================================================================
 * 文件: album.c
 * 说明: 相册浏览模块实现
 *      功能:
 *        1. 启动默认进入"缩略图网格"模式
 *        2. 点击缩略图 → 进入"单图查看"模式, 等比居中显示
 *        3. 单图模式:
 *             - 点击图片 → 放大/缩小(1.0x ↔ 2.0x 切换)
 *             - 滑动左右 → 切换上一张/下一张
 *             - 点击右上角 "<<" 按钮 → 返回缩略图
 *             - 点击左上角 "<" → 退出相册
 *      资源: 扫描目录中 *.bmp, 排序后作为图片列表
 * ==========================================================================*/
#include "album.h"
#include "app_ui.h"
#include "ui.h"
#include "input.h"
#include "bmp_display.h"   /* bmp_info_t 等类型直接依赖 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#define MAX_FILES  256
#define PATH_MAX_LEN 512

/* 顶部/底部栏高 */
#define ALBUM_TOPBAR_H  60
#define ALBUM_BOTBAR_H  40

/* 缩略图网格参数 */
#define THUMB_COLS  4
#define THUMB_GAP   12
#define THUMB_MARGIN_X  16
#define THUMB_MARGIN_Y_TOP  (ALBUM_TOPBAR_H + 16)
#define THUMB_MARGIN_Y_BOT  (ALBUM_BOTBAR_H + 16)

/* 图片数据 */
static char  g_files[MAX_FILES][PATH_MAX_LEN];
static int   g_thumb_w[MAX_FILES], g_thumb_h[MAX_FILES];
static unsigned char *g_thumb_data[MAX_FILES];   /* RGB 缩略图缓存 */
static int   g_thumb_row[MAX_FILES];              /* row_size */
static int   g_file_count = 0;
static int   g_cur_index  = 0;

/* 视图模式 */
typedef enum { VIEW_THUMBS, VIEW_SINGLE } view_mode_t;
static view_mode_t g_mode = VIEW_THUMBS;
/* 单图模式: 缩放 1.0 原始, 2.0 放大 */
static int   g_zoomed = 0;

/* 工具栏按钮 */
static ui_button_t g_back_btn;       /* 左上: 退出相册 */
static ui_button_t g_thumbs_btn;     /* 右上: 返回缩略图(单图模式) */

/* ---------- 工具 ---------- */
static void sort_strings(char arr[][PATH_MAX_LEN], int n)
{
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - 1 - i; j++) {
            if (strcmp(arr[j], arr[j + 1]) > 0) {
                char tmp[PATH_MAX_LEN];
                strcpy(tmp, arr[j]);
                strcpy(arr[j], arr[j + 1]);
                strcpy(arr[j + 1], tmp);
            }
        }
    }
}

static int is_bmp_file(const char *name)
{
    size_t len = strlen(name);
    if (len < 4) return 0;
    const char *s = name + len - 4;
    return (s[0] == '.' &&
           (s[1] == 'b' || s[1] == 'B') &&
           (s[2] == 'm' || s[2] == 'M') &&
           (s[3] == 'p' || s[3] == 'P'));
}

/* 扫描目录 */
static int scan_dir(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "[album] 无法打开目录 %s: %s\n", dir, strerror(errno));
        return -1;
    }
    g_file_count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL && g_file_count < MAX_FILES) {
        if (de->d_name[0] == '.') continue;
        if (!is_bmp_file(de->d_name)) continue;
        snprintf(g_files[g_file_count], PATH_MAX_LEN,
                 "%s/%s", dir, de->d_name);
        g_file_count++;
    }
    closedir(d);
    sort_strings(g_files, g_file_count);
    return (g_file_count > 0) ? 0 : -1;
}

/* 预生成所有缩略图 (懒加载也可以, 这里简单些) */
static void gen_thumbnails(void)
{
    int cell_w = (g_screen_w - 2 * THUMB_MARGIN_X
                  - (THUMB_COLS - 1) * THUMB_GAP) / THUMB_COLS;
    int avail_h = g_screen_h - THUMB_MARGIN_Y_TOP - THUMB_MARGIN_Y_BOT;
    int rows = (g_file_count + THUMB_COLS - 1) / THUMB_COLS;
    if (rows < 1) rows = 1;
    int cell_h = (avail_h - (rows - 1) * THUMB_GAP) / rows;
    if (cell_h > cell_w) cell_h = cell_w;
    if (cell_h < 40)     cell_h = 40;

    int target_w = cell_w - 8;
    int target_h = cell_h - 8;

    for (int i = 0; i < g_file_count; i++) {
        g_thumb_data[i] = NULL;
        g_thumb_w[i] = g_thumb_h[i] = 0;
        g_thumb_row[i] = 0;
        bmp_info_t info;
        if (read_bmp_info(g_files[i], &info) < 0) continue;
        unsigned char *pix = read_bmp_pixels(g_files[i], &info);
        if (!pix) continue;

        /* 等比缩放到 target */
        double s_w = (double)target_w / info.width;
        double s_h = (double)target_h / info.height;
        double s   = (s_w < s_h) ? s_w : s_h;
        if (s > 1.0) s = 1.0;
        unsigned int dw = (unsigned int)(info.width * s);
        unsigned int dh = (unsigned int)(info.height * s);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;

        /* 居中放缩略图到 cell */
        int cell_total_w = dw;
        int cell_total_h = dh;
        unsigned char *buf = malloc(cell_total_w * cell_total_h * 3);
        if (!buf) { free(pix); continue; }
        /* 清为黑 */
        memset(buf, 0, cell_total_w * cell_total_h * 3);

        /* 缩放取像素 (最近邻) */
        for (unsigned int y = 0; y < dh; y++) {
            unsigned int sy2 = (unsigned int)((double)y / s);
            if (sy2 >= info.height) sy2 = info.height - 1;
            int src_row = info.bottom_up ? ((int)info.height - 1 - (int)sy2)
                                          : (int)sy2;
            const unsigned char *src = pix + src_row * info.row_size;
            for (unsigned int x = 0; x < dw; x++) {
                unsigned int sx2 = (unsigned int)((double)x / s);
                if (sx2 >= info.width) sx2 = info.width - 1;
                const unsigned char *sp = src + sx2 * 3;
                unsigned char *dp = buf + (y * cell_total_w + x) * 3;
                dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
            }
        }
        free(pix);
        g_thumb_data[i] = buf;
        g_thumb_w[i] = cell_total_w;
        g_thumb_h[i] = cell_total_h;
        g_thumb_row[i] = cell_total_w * 3;
    }
}

static void free_thumbnails(void)
{
    for (int i = 0; i < g_file_count; i++) {
        if (g_thumb_data[i]) {
            free(g_thumb_data[i]);
            g_thumb_data[i] = NULL;
        }
    }
}

/* ---------- 绘制 ---------- */
static void draw_topbar(void)
{
    ui_fill_rect(0, 0, g_screen_w, ALBUM_TOPBAR_H, APP_COLOR_HEADER);
    /* 返回按钮 (左上) */
    g_back_btn.x = 10;
    g_back_btn.y = 10;
    g_back_btn.w = ALBUM_TOPBAR_H - 20;
    g_back_btn.h = ALBUM_TOPBAR_H - 20;
    g_back_btn.label = "<";
    g_back_btn.id = 1;
    ui_draw_button(&g_back_btn, APP_COLOR_SURFACE, APP_COLOR_BORDER,
                   UI_COLOR_WHITE, APP_COLOR_HEADER, 2);

    /* 单图模式: 缩略图列表按钮 (右上) */
    if (g_mode == VIEW_SINGLE) {
        g_thumbs_btn.x = g_screen_w - ALBUM_TOPBAR_H + 10;
        g_thumbs_btn.y = 10;
        g_thumbs_btn.w = ALBUM_TOPBAR_H - 20;
        g_thumbs_btn.h = ALBUM_TOPBAR_H - 20;
        g_thumbs_btn.label = "[]";
        g_thumbs_btn.id = 2;
        ui_draw_button(&g_thumbs_btn, APP_COLOR_PRIMARY, APP_COLOR_BORDER,
                       APP_COLOR_BG, APP_COLOR_HEADER, 2);
    }

    /* 标题 */
    char title[64];
    if (g_file_count > 0) {
        if (g_mode == VIEW_SINGLE) {
            snprintf(title, sizeof(title), "%d/%d  %s",
                     g_cur_index + 1, g_file_count,
                     g_zoomed ? "[2x]" : "[1x]");
        } else {
            snprintf(title, sizeof(title), "Album  %d pics", g_file_count);
        }
    } else {
        snprintf(title, sizeof(title), "Album  (empty)");
    }
    int title_w = (int)strlen(title) * 8 * 2;
    int title_x = g_screen_w / 2 - title_w / 2;
    ui_draw_text(title_x, (ALBUM_TOPBAR_H - 16) / 2,
                 title, UI_COLOR_WHITE, APP_COLOR_HEADER, 2);
}

static void draw_botbar(void)
{
    int y0 = g_screen_h - ALBUM_BOTBAR_H;
    ui_fill_rect(0, y0, g_screen_w, ALBUM_BOTBAR_H, APP_COLOR_HEADER);
    const char *hint = (g_mode == VIEW_THUMBS)
        ? "<- Back | Tap thumbnail to open"
        : "<- Back | Tap image to zoom 2x | Swipe L/R to switch | [] to thumbs";
    ui_draw_text(20, y0 + (ALBUM_BOTBAR_H - 16) / 2,
                 (char *)hint, APP_COLOR_MUTED, APP_COLOR_HEADER, 1);
}

/* 缩略图网格视图 */
static void draw_thumb_view(void)
{
    int avail_w = g_screen_w;
    int avail_h = g_screen_h - THUMB_MARGIN_Y_TOP - THUMB_MARGIN_Y_BOT;
    if (avail_h < 100) avail_h = 100;
    int cell_w = (avail_w - 2 * THUMB_MARGIN_X
                  - (THUMB_COLS - 1) * THUMB_GAP) / THUMB_COLS;
    int rows = (g_file_count + THUMB_COLS - 1) / THUMB_COLS;
    if (rows < 1) rows = 1;
    int cell_h = (avail_h - (rows - 1) * THUMB_GAP) / rows;
    if (cell_h > cell_w) cell_h = cell_w;
    if (cell_h < 40)     cell_h = 40;

    /* 画布底色 */
    ui_fill_rect(0, ALBUM_TOPBAR_H, g_screen_w,
                 g_screen_h - ALBUM_TOPBAR_H, UI_COLOR_BLACK);

    /* 缩略图 */
    for (int i = 0; i < g_file_count; i++) {
        int r = i / THUMB_COLS;
        int c = i % THUMB_COLS;
        int x = THUMB_MARGIN_X + c * (cell_w + THUMB_GAP);
        int y = THUMB_MARGIN_Y_TOP + r * (cell_h + THUMB_GAP);

        /* 边框 (格) */
        ui_fill_rect(x, y, cell_w, cell_h, APP_COLOR_PANEL);
        ui_draw_rect(x, y, cell_w, cell_h, APP_COLOR_BORDER);

        if (!g_thumb_data[i]) {
            /* 占位 */
            const char *err = "N/A";
            int ew = (int)strlen(err) * 8;
            ui_draw_text(x + (cell_w - ew) / 2,
                         y + cell_h / 2 - 8, err,
                         UI_COLOR_LGRAY, 0xFF000000u, 1);
            continue;
        }
        /* 居中绘制缩略图 */
        int dw = g_thumb_w[i], dh = g_thumb_h[i];
        int px = x + (cell_w - dw) / 2;
        int py = y + (cell_h - dh) / 2;
        for (int yy = 0; yy < dh; yy++) {
            int sy = py + yy;
            if (sy < 0 || sy >= g_screen_h) continue;
            const unsigned char *row = g_thumb_data[i] + yy * g_thumb_row[i];
            for (int xx = 0; xx < dw; xx++) {
                int sx = px + xx;
                if (sx < 0 || sx >= g_screen_w) continue;
                unsigned int c2 = 0xFF000000u
                    | ((unsigned int)row[xx * 3 + 2] << 16)
                    | ((unsigned int)row[xx * 3 + 1] << 8)
                    |  (unsigned int)row[xx * 3 + 0];
                ui_put_pixel(sx, sy, c2);
            }
        }
        /* 编号 */
        char idx[16];
        snprintf(idx, sizeof(idx), "%d", i + 1);
        ui_draw_text(x + 4, y + 4, idx,
                     UI_COLOR_YELLOW, 0xFF000000u, 1);
    }
}

/* 单图视图: 缩放 + 居中 */
static void draw_single_view(void)
{
    if (g_file_count == 0) {
        ui_fill_rect(0, ALBUM_TOPBAR_H, g_screen_w,
                     g_screen_h - ALBUM_TOPBAR_H, UI_COLOR_BLACK);
        const char *msg = "No BMP files";
        int mw = (int)strlen(msg) * 8 * 2;
        ui_draw_text(g_screen_w / 2 - mw / 2,
                     (ALBUM_TOPBAR_H + g_screen_h) / 2 - 16,
                     msg, UI_COLOR_WHITE, 0xFF000000u, 2);
        return;
    }

    bmp_info_t info;
    if (read_bmp_info(g_files[g_cur_index], &info) < 0) {
        ui_fill_rect(0, ALBUM_TOPBAR_H, g_screen_w,
                     g_screen_h - ALBUM_TOPBAR_H, UI_COLOR_BLACK);
        const char *msg = "Failed to load image";
        int mw = (int)strlen(msg) * 8 * 2;
        ui_draw_text(g_screen_w / 2 - mw / 2,
                     (ALBUM_TOPBAR_H + g_screen_h) / 2 - 16,
                     msg, UI_COLOR_RED, 0xFF000000u, 2);
        return;
    }

    int avail_w = g_screen_w;
    int avail_h = g_screen_h - ALBUM_TOPBAR_H - ALBUM_BOTBAR_H;
    double base_scale_w = (double)avail_w / info.width;
    double base_scale_h = (double)avail_h / info.height;
    double base_scale   = (base_scale_w < base_scale_h) ? base_scale_w : base_scale_h;
    if (base_scale > 1.0) base_scale = 1.0;
    if (base_scale < 0.05) base_scale = 0.05;

    /* 缩放: 1x = base_scale; 2x = base*2 (允许超出屏幕) */
    double scale = g_zoomed ? base_scale * 2.0 : base_scale;

    unsigned char *pixels = read_bmp_pixels(g_files[g_cur_index], &info);
    if (!pixels) return;

    unsigned int draw_w = info.width, draw_h = info.height;
    unsigned char *scaled = NULL;
    unsigned char *src = pixels;
    unsigned int src_row_sz = info.row_size;
    if (scale < 0.999 || scale > 1.001) {
        scaled = scale_bmp_nearest(src, info.width, info.height,
                                   src_row_sz, scale, &draw_w, &draw_h);
        if (scaled) { src = scaled; src_row_sz = draw_w * 3; }
    }

    /* 居中 (放大模式可能超界, 允许) */
    int draw_x = (g_screen_w - (int)draw_w) / 2;
    int draw_y = ALBUM_TOPBAR_H + (avail_h - (int)draw_h) / 2;

    /* 画布背景 */
    ui_fill_rect(0, ALBUM_TOPBAR_H, g_screen_w, avail_h, UI_COLOR_BLACK);

    for (unsigned int dy = 0; dy < draw_h; dy++) {
        int sy = draw_y + (int)dy;
if (sy < ALBUM_TOPBAR_H || sy >= g_screen_h - ALBUM_BOTBAR_H) continue;
        int src_row;
        if (scaled) {
            src_row = info.bottom_up ? ((int)draw_h - 1 - (int)dy)
                                     : (int)dy;
        } else {
            src_row = info.bottom_up ? ((int)info.height - 1 - (int)dy)
                                     : (int)dy;
        }
        const unsigned char *row = src + src_row * src_row_sz;
        for (unsigned int dx = 0; dx < draw_w; dx++) {
            int sx = draw_x + (int)dx;
            if (sx < 0 || sx >= g_screen_w) continue;
            unsigned int color = 0xFF000000u
                | ((unsigned int)row[dx * 3 + 2] << 16)
                | ((unsigned int)row[dx * 3 + 1] << 8)
                |  (unsigned int)row[dx * 3 + 0];
            ui_put_pixel(sx, sy, color);
        }
    }
    if (scaled) free(scaled);
    free(pixels);
}

static void redraw(void)
{
    if (g_mode == VIEW_THUMBS) draw_thumb_view();
    else                       draw_single_view();
    draw_topbar();
    draw_botbar();
}

/* 命中缩略图: 返回 -1 未命中 */
static int hit_thumb(int x, int y)
{
    int avail_w = g_screen_w;
    int avail_h = g_screen_h - THUMB_MARGIN_Y_TOP - THUMB_MARGIN_Y_BOT;
    if (avail_h < 100) avail_h = 100;
    int cell_w = (avail_w - 2 * THUMB_MARGIN_X
                  - (THUMB_COLS - 1) * THUMB_GAP) / THUMB_COLS;
    int rows = (g_file_count + THUMB_COLS - 1) / THUMB_COLS;
    if (rows < 1) rows = 1;
    int cell_h = (avail_h - (rows - 1) * THUMB_GAP) / rows;
    if (cell_h > cell_w) cell_h = cell_w;
    if (cell_h < 40)     cell_h = 40;

    for (int i = 0; i < g_file_count; i++) {
        int r = i / THUMB_COLS;
        int c = i % THUMB_COLS;
        int cx = THUMB_MARGIN_X + c * (cell_w + THUMB_GAP);
        int cy = THUMB_MARGIN_Y_TOP + r * (cell_h + THUMB_GAP);
        if (x >= cx && x < cx + cell_w &&
            y >= cy && y < cy + cell_h) {
            return i;
        }
    }
    return -1;
}

int album_run(const char *dir)
{
    const char *target = (dir && dir[0]) ? dir : ALBUM_DEFAULT_DIR;
    if (scan_dir(target) < 0) {
        g_file_count = 0;
    }
    g_cur_index = 0;
    g_mode = VIEW_THUMBS;
    g_zoomed = 0;

    gen_thumbnails();
    redraw();

    for (;;) {
        input_event_t ev;
        int r = input_wait_event(&ev, 1000);
        if (r == 0 && ev.type == INPUT_QUIT) return 0;
        if (r < 0) continue;

        if (ev.type == INPUT_KEY_BACK) return 0;

        if (ev.type == INPUT_TAP) {
            /* 顶部返回 */
            if (ui_button_hit(&g_back_btn, ev.x, ev.y)) {
                if (g_mode == VIEW_SINGLE) {
                    /* 单图模式: 返回缩略图 */
                    g_mode = VIEW_THUMBS;
                    redraw();
                } else {
                    return 0;  /* 退出相册 */
                }
                continue;
            }
            /* 缩略图按钮(单图模式右上) */
            if (g_mode == VIEW_SINGLE &&
                ui_button_hit(&g_thumbs_btn, ev.x, ev.y)) {
                g_mode = VIEW_THUMBS;
                redraw();
                continue;
            }
            /* 主体区域 */
            if (ev.y < ALBUM_TOPBAR_H) continue;
            if (ev.y > g_screen_h - ALBUM_BOTBAR_H) continue;

            if (g_mode == VIEW_THUMBS) {
                int idx = hit_thumb(ev.x, ev.y);
                if (idx >= 0 && g_thumb_data[idx]) {
                    g_cur_index = idx;
                    g_mode = VIEW_SINGLE;
                    g_zoomed = 0;
                    redraw();
                }
            } else {
                /* 单图: 点击切换 1x / 2x 缩放 */
                g_zoomed = !g_zoomed;
                redraw();
            }
        } else if (g_mode == VIEW_SINGLE) {
            /* 单图: 滑动切图 */
            if (ev.type == INPUT_SWIPE_LEFT) {
                g_cur_index = (g_cur_index + 1) % g_file_count;
                g_zoomed = 0;
                redraw();
            } else if (ev.type == INPUT_SWIPE_RIGHT) {
                g_cur_index = (g_cur_index - 1 + g_file_count) % g_file_count;
                g_zoomed = 0;
                redraw();
            } else if (ev.type == INPUT_SWIPE_UP ||
                       ev.type == INPUT_SWIPE_DOWN) {
                /* 上滑/下滑 → 切换缩放 */
                g_zoomed = !g_zoomed;
                redraw();
            }
        }
    }
}
