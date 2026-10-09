/* ============================================================================
 * 文件: paint.c
 * 说明: 画板实现
 *      实现原理:
 *        - 使用 input_poll_raw 读取连续触摸事件 (DOWN/MOVE/UP)
 *        - MOVE 事件在两点之间用 Bresenham 画线, 避免漏点
 *        - 撤销栈: 保存每笔的 stroke (x,y,color,thick) 列表, 撤销时重画整个背景
 *          然后按顺序重放 stroke
 * ==========================================================================*/
#include "paint.h"
#include "app_ui.h"
#include "ui.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 调色板 */
#define PALETTE_N  8
static const unsigned int PALETTE[PALETTE_N] = {
    0xFF000000u,  /* 黑 */
    0xFFFFFFFFu,  /* 白 */
    0xFFFF0000u,  /* 红 */
    0xFF00FF00u,  /* 绿 */
    0xFF0000FFu,  /* 蓝 */
    0xFFFFFF00u,  /* 黄 */
    0xFFFF00FFu,  /* 紫 */
    0xFF00FFFFu,  /* 青 */
};

/* 工具栏尺寸 */
#define PALETTE_H  50
#define TOOLBAR_H  50

/* 笔粗档位 */
#define BRUSH_N    3
static const int BRUSH_W[BRUSH_N] = { 2, 5, 10 };
static const char *BRUSH_LBL[BRUSH_N] = { "S", "M", "L" };

/* 单笔最大点数 (触摸采样, 16ms 一帧大概 60 点/秒) */
#define STROKE_MAX_PT  2048

/* 撤销栈: 最多 30 笔 */
#define HISTORY_MAX 30

/* 一笔 (stroke) */
typedef struct {
    short x[STROKE_MAX_PT];
    short y[STROKE_MAX_PT];
    int   n;
    unsigned int color;
    int   thick;
} stroke_t;

/* 撤销栈: 整个背景 + 笔列表 */
static stroke_t g_history[HISTORY_MAX];
static int      g_history_n = 0;
static int      g_cur_color = 2;  /* 默认红 */
static int      g_cur_brush = 1;  /* 默认中 */

static unsigned int g_canvas_bg;  /* 当前画布底色 (清空时重置) */

/* 工具: 画粗点 (圆刷) */
static void brush_dot(int x, int y, int r, unsigned int c)
{
    if (r <= 0) { ui_put_pixel(x, y, c); return; }
    ui_fill_circle(x, y, r, c);
}

/* 工具: Bresenham 画线, 沿线用 brush_dot 画粗点 */
static void draw_brush_line(int x0, int y0, int x1, int y1,
                             int r, unsigned int c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        brush_dot(x0, y0, r, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* 重放历史 (清空画布后) */
static void redraw_all(void)
{
    /* 重画画布背景 */
    int x0 = 0, y0 = PALETTE_H;
    int w = g_screen_w, h = g_screen_h - PALETTE_H - TOOLBAR_H;
    ui_fill_rect(x0, y0, w, h, g_canvas_bg);
    for (int i = 0; i < g_history_n; i++) {
        stroke_t *s = &g_history[i];
        for (int j = 1; j < s->n; j++) {
            draw_brush_line(s->x[j - 1], s->y[j - 1],
                            s->x[j],     s->y[j],
                            s->thick, s->color);
        }
        if (s->n == 1) brush_dot(s->x[0], s->y[0], s->thick, s->color);
    }
}

/* 绘制顶部调色板 */
static void draw_palette(void)
{
    ui_fill_rect(0, 0, g_screen_w, PALETTE_H, APP_COLOR_HEADER);
    int cw = g_screen_w / PALETTE_N;
    for (int i = 0; i < PALETTE_N; i++) {
        int x = i * cw;
        int y = 5;
        int h = PALETTE_H - 10;
        ui_fill_rect(x + 4, y, cw - 8, h, PALETTE[i]);
        if (i == g_cur_color) {
            ui_draw_rect(x + 2, y - 2, cw - 4, h + 4, UI_COLOR_WHITE);
        }
    }
}

/* 绘制底部工具栏 */
static void draw_toolbar(void)
{
    int y0 = g_screen_h - TOOLBAR_H;
    ui_fill_rect(0, y0, g_screen_w, TOOLBAR_H, APP_COLOR_HEADER);

    int x = 10;
    int bw = 80, bh = TOOLBAR_H - 10;
    /* 笔粗档位 */
    for (int i = 0; i < BRUSH_N; i++) {
        ui_button_t b = { x, y0 + 5, bw, bh, (char *)BRUSH_LBL[i], 200 + i };
        unsigned int fill = (i == g_cur_brush) ? APP_COLOR_PRIMARY
                                               : APP_COLOR_SURFACE;
        ui_draw_button(&b, fill, APP_COLOR_BORDER,
                       (i == g_cur_brush) ? APP_COLOR_BG : UI_COLOR_WHITE,
                       APP_COLOR_HEADER, 2);
        x += bw + 8;
    }
    /* 撤销 */
    { ui_button_t b = { x, y0 + 5, bw, bh, "Undo", 210 };
      ui_draw_button(&b, APP_COLOR_SURFACE, APP_COLOR_BORDER,
                     UI_COLOR_WHITE, APP_COLOR_HEADER, 2);
      x += bw + 8; }
    /* 清空 */
    { ui_button_t b = { x, y0 + 5, bw, bh, "Clear", 211 };
      ui_draw_button(&b, APP_COLOR_SURFACE, APP_COLOR_BORDER,
                     UI_COLOR_WHITE, APP_COLOR_HEADER, 2);
      x += bw + 8; }
    /* 返回 */
    { ui_button_t b = { x, y0 + 5, bw + 40, bh, "Back", 212 };
      ui_draw_button(&b, APP_COLOR_DANGER, APP_COLOR_BORDER,
                     UI_COLOR_WHITE, APP_COLOR_HEADER, 2); }
}

/* 命中调色板: 返回 0..PALETTE_N-1, -1 未命中 */
static int hit_palette(int x, int y)
{
    if (y < 0 || y >= PALETTE_H) return -1;
    if (x < 0 || x >= g_screen_w) return -1;
    return x / (g_screen_w / PALETTE_N);
}

/* 命中工具栏: 返回 id, -1 未命中 */
static int hit_toolbar(int x, int y)
{
    int y0 = g_screen_h - TOOLBAR_H;
    if (y < y0 || y >= g_screen_h) return -1;
    int x0 = 10;
    int bw = 80, gap = 8;

    /* 6 个按钮: S / M / L / Undo / Clear / Back */
    int bxs[6] = { x0,
                   x0 + 1 * (bw + gap),
                   x0 + 2 * (bw + gap),
                   x0 + 3 * (bw + gap),
                   x0 + 4 * (bw + gap),
                   x0 + 5 * (bw + gap) };
    int widths[6] = { bw, bw, bw, bw, bw, bw + 40 };
    int ids   [6] = { 200, 201, 202, 210, 211, 212 };
    for (int i = 0; i < 6; i++) {
        if (x >= bxs[i] && x < bxs[i] + widths[i]) {
            return ids[i];
        }
    }
    return -1;
}

int paint_run(void)
{
    /* 初始化 */
    g_history_n = 0;
    g_cur_color = 2;
    g_cur_brush = 1;
    g_canvas_bg = UI_COLOR_WHITE;

    ui_clear(UI_COLOR_BLACK);
    /* 画布区填充 */
    ui_fill_rect(0, PALETTE_H, g_screen_w,
                 g_screen_h - PALETTE_H - TOOLBAR_H, g_canvas_bg);
    draw_palette();
    draw_toolbar();

    /* 当前正在绘制的笔 */
    stroke_t cur;
    cur.n = 0;
    int drawing = 0;

    for (;;) {
        input_event_t ev;
        int r = input_poll_raw(&ev);
        if (r < 0) {
            usleep(10000);
            /* 仍可处理 toolbar 区域 TAP? 简单起见: 不处理 */
            continue;
        }
        if (r == 0 && ev.type == INPUT_NONE) {
            usleep(8000);
            continue;
        }
        if (ev.type == INPUT_QUIT) return -1;
        if (ev.type == INPUT_KEY_BACK) return 0;

        if (ev.type == INPUT_TAP) {
            /* 如果当前正在绘制 (DOWN 已开笔), 则 TAP 相当于一次快速释放,
             * 把当前笔直接保存到历史并结束. */
            if (drawing) {
                drawing = 0;
                if (cur.n > 0) {
                    if (g_history_n < HISTORY_MAX) {
                        g_history[g_history_n++] = cur;
                    } else {
                        for (int i = 1; i < HISTORY_MAX; i++)
                            g_history[i - 1] = g_history[i];
                        g_history[HISTORY_MAX - 1] = cur;
                    }
                }
                cur.n = 0;
                continue;
            }
            /* 调色板? */
            int p = hit_palette(ev.x, ev.y);
            if (p >= 0) {
                g_cur_color = p;
                draw_palette();
                continue;
            }
            /* 工具栏? */
            int t = hit_toolbar(ev.x, ev.y);
            if (t >= 0) {
                if (t == 212) return 0;          /* Back */
                if (t >= 200 && t < 203) {        /* 笔粗 */
                    g_cur_brush = t - 200;
                    draw_toolbar();
                    continue;
                }
                if (t == 210) {                   /* Undo */
                    if (g_history_n > 0) g_history_n--;
                    redraw_all();
                    continue;
                }
                if (t == 211) {                   /* Clear */
                    g_history_n = 0;
                    redraw_all();
                    continue;
                }
            }
            /* 画布区: 落笔 (单点) */
            if (ev.y >= PALETTE_H &&
                ev.y < g_screen_h - TOOLBAR_H) {
                cur.n = 0;
                cur.color = PALETTE[g_cur_color];
                cur.thick = BRUSH_W[g_cur_brush];
                cur.x[cur.n] = ev.x;
                cur.y[cur.n] = ev.y;
                cur.n++;
                brush_dot(ev.x, ev.y, cur.thick, cur.color);
                /* 单点也保存为一次"笔", 方便 Undo 撤回 */
                if (g_history_n < HISTORY_MAX) {
                    g_history[g_history_n++] = cur;
                } else {
                    for (int i = 1; i < HISTORY_MAX; i++)
                        g_history[i - 1] = g_history[i];
                    g_history[HISTORY_MAX - 1] = cur;
                }
            }
        } else if (ev.type == INPUT_TOUCH_DOWN) {
            if (ev.y >= PALETTE_H &&
                ev.y < g_screen_h - TOOLBAR_H) {
                drawing = 1;
                cur.n = 0;
                cur.color = PALETTE[g_cur_color];
                cur.thick = BRUSH_W[g_cur_brush];
                cur.x[cur.n] = ev.x;
                cur.y[cur.n] = ev.y;
                cur.n++;
                brush_dot(ev.x, ev.y, cur.thick, cur.color);
            }
        } else if (ev.type == INPUT_TOUCH_MOVE) {
            if (drawing && cur.n < STROKE_MAX_PT &&
                ev.y >= PALETTE_H &&
                ev.y < g_screen_h - TOOLBAR_H) {
                int px = cur.x[cur.n - 1];
                int py = cur.y[cur.n - 1];
                draw_brush_line(px, py, ev.x, ev.y,
                                cur.thick, cur.color);
                cur.x[cur.n] = ev.x;
                cur.y[cur.n] = ev.y;
                cur.n++;
            }
        } else if (ev.type == INPUT_TOUCH_UP) {
            if (drawing) {
                drawing = 0;
                if (cur.n > 0) {
                    if (g_history_n < HISTORY_MAX) {
                        g_history[g_history_n++] = cur;
                    } else {
                        /* 丢弃最老 */
                        for (int i = 1; i < HISTORY_MAX; i++)
                            g_history[i - 1] = g_history[i];
                        g_history[HISTORY_MAX - 1] = cur;
                    }
                }
            }
        }
    }
}
