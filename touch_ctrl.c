/* ============================================================================
 * 文件: touch_ctrl.c
 * 说明: 虚拟触控按键组件实现
 *      - 使用 UI 原语绘制半透明方向键和动作键
 *      - D-Pad: 十字布局, 箭头三角标识方向
 *      - 动作键: 圆形按钮
 *      - 所有按键大小按屏幕尺寸自适应
 * ==========================================================================*/
#include "touch_ctrl.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 按键区域结构 */
typedef struct {
    int x, y, w, h;
    vkey_t key;
} vkey_rect_t;

static vkey_rect_t g_vkeys[VKEY_COUNT];
static int         g_vkey_count = 0;
static vlayout_t   g_layout;
static int         g_bottom_reserve = 0;

/* D-Pad 自适应大小: 按屏幕短边比例 */
static int dpad_size(void)
{
    int s = (g_screen_w < g_screen_h ? g_screen_w : g_screen_h) / 6;
    if (s < 60)  s = 60;
    if (s > 120) s = 120;
    return s;
}

/* 动作键大小 */
static int action_size(void)
{
    int s = dpad_size();
    return s * 3 / 4;
}

/* 半透明颜色混合 (alpha: 0..255) */
static unsigned int alpha_blend(unsigned int fg, unsigned int bg, int alpha)
{
    unsigned int fa = (fg >> 24) & 0xFF;
    unsigned int fr = (fg >> 16) & 0xFF;
    unsigned int fg_ = (fg >> 8) & 0xFF;
    unsigned int fb = fg & 0xFF;

    unsigned int ba = (bg >> 24) & 0xFF;
    unsigned int br = (bg >> 16) & 0xFF;
    unsigned int bg_ = (bg >> 8) & 0xFF;
    unsigned int bb = bg & 0xFF;

    unsigned int a = (fa * alpha + ba * (255 - alpha)) / 255;
    unsigned int r = (fr * alpha + br * (255 - alpha)) / 255;
    unsigned int g = (fg_ * alpha + bg_ * (255 - alpha)) / 255;
    unsigned int b = (fb * alpha + bb * (255 - alpha)) / 255;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* 在指定位置绘制一个实心小三角 */
static void draw_triangle_up(int cx, int cy, int half, unsigned int color)
{
    for (int dy = 0; dy <= half; dy++) {
        int dx = half - dy;
        for (int x = cx - dx; x <= cx + dx; x++) {
            ui_put_pixel(x, cy - dy, color);
        }
    }
}

static void draw_triangle_down(int cx, int cy, int half, unsigned int color)
{
    for (int dy = 0; dy <= half; dy++) {
        int dx = half - dy;
        for (int x = cx - dx; x <= cx + dx; x++) {
            ui_put_pixel(x, cy + dy, color);
        }
    }
}

static void draw_triangle_left(int cx, int cy, int half, unsigned int color)
{
    for (int dx = 0; dx <= half; dx++) {
        int dy = half - dx;
        for (int y = cy - dy; y <= cy + dy; y++) {
            ui_put_pixel(cx - dx, y, color);
        }
    }
}

static void draw_triangle_right(int cx, int cy, int half, unsigned int color)
{
    for (int dx = 0; dx <= half; dx++) {
        int dy = half - dx;
        for (int y = cy - dy; y <= cy + dy; y++) {
            ui_put_pixel(cx + dx, y, color);
        }
    }
}

/* 绘制单方向键 */
static void draw_dpad_button(int x, int y, int w, int h, vkey_t key,
                             unsigned int fill, unsigned int arrow_c)
{
    /* 半透明填充 */
    int bx = x, by = y, bw = w, bh = h;
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;
    if (bx + bw > g_screen_w) bw = g_screen_w - bx;
    if (by + bh > g_screen_h) bh = g_screen_h - by;
    if (bw <= 0 || bh <= 0) return;

    /* 圆角效果: 画矩形 + 四个角用背景色修正 (偷懒: 直接矩形) */
    ui_fill_rect(bx + 2, by, bw - 4, bh, fill);
    ui_fill_rect(bx, by + 2, bw, bh - 4, fill);
    ui_draw_rect(bx, by, bw, bh, 0x88FFFFFFu);

    /* 箭头 */
    int cx = bx + bw / 2;
    int cy = by + bh / 2;
    int half = (bw < bh ? bw : bh) / 6;
    if (half < 6) half = 6;

    switch (key) {
    case VKEY_UP:    draw_triangle_up(cx, cy + half / 2, half, arrow_c); break;
    case VKEY_DOWN:  draw_triangle_down(cx, cy - half / 2, half, arrow_c); break;
    case VKEY_LEFT:  draw_triangle_left(cx + half / 2, cy, half, arrow_c); break;
    case VKEY_RIGHT: draw_triangle_right(cx - half / 2, cy, half, arrow_c); break;
    default: break;
    }
}

/* 绘制圆形动作键 */
static void draw_action_button(int cx, int cy, int r, const char *label,
                               unsigned int fill, unsigned int text_c)
{
    ui_fill_circle(cx, cy, r, fill);
    ui_fill_rect(cx - r, cy - r, 2 * r, 2 * r, 0x44FFFFFFu);  /* 外圈 */
    if (label) {
        int tx = cx - (int)(strlen(label) * 8) / 2;
        /* 简易文字居中 (8x16 字体) */
        ui_draw_text(tx, cy - 8, label, text_c, 0xFF000000u, 1);
    }
}

/* 初始化: 计算所有虚拟按键位置 */
void touch_ctrl_init(vlayout_t layout, int bottom_reserve)
{
    g_layout = layout;
    g_bottom_reserve = bottom_reserve;
    g_vkey_count = 0;

    int ds = dpad_size();
    int as = action_size();
    int margin = 20;
    int gap = 8;
    int br = g_bottom_reserve;  /* 底部保留区域 */

    switch (layout) {
    case VLAYOUT_DPAD_ONLY: {
        /* 十字 D-Pad 放在左下角 */
        int cx = margin + ds;
        int cy = g_screen_h - margin - ds - br;
        int hs = ds / 2;

        /* 上键 */
        g_vkeys[0].x = cx - hs;  g_vkeys[0].y = cy - ds;
        g_vkeys[0].w = ds;       g_vkeys[0].h = ds;
        g_vkeys[0].key = VKEY_UP;

        /* 下键 */
        g_vkeys[1].x = cx - hs;  g_vkeys[1].y = cy + hs;
        g_vkeys[1].w = ds;       g_vkeys[1].h = ds;
        g_vkeys[1].key = VKEY_DOWN;

        /* 左键 */
        g_vkeys[2].x = cx - ds;  g_vkeys[2].y = cy - hs;
        g_vkeys[2].w = ds;       g_vkeys[2].h = ds;
        g_vkeys[2].key = VKEY_LEFT;

        /* 右键 */
        g_vkeys[3].x = cx + hs;  g_vkeys[3].y = cy - hs;
        g_vkeys[3].w = ds;       g_vkeys[3].h = ds;
        g_vkeys[3].key = VKEY_RIGHT;

        g_vkey_count = 4;
        break;
    }

    case VLAYOUT_DPAD_ACTIONS: {
        /* 十字 D-Pad 左下 + 两颗动作键右下 */
        int cx = margin + ds;
        int cy = g_screen_h - margin - ds - br;
        int hs = ds / 2;

        /* D-Pad 四键 */
        g_vkeys[0].x = cx - hs;  g_vkeys[0].y = cy - ds;
        g_vkeys[0].w = ds;       g_vkeys[0].h = ds;
        g_vkeys[0].key = VKEY_UP;

        g_vkeys[1].x = cx - hs;  g_vkeys[1].y = cy + hs;
        g_vkeys[1].w = ds;       g_vkeys[1].h = ds;
        g_vkeys[1].key = VKEY_DOWN;

        g_vkeys[2].x = cx - ds;  g_vkeys[2].y = cy - hs;
        g_vkeys[2].w = ds;       g_vkeys[2].h = ds;
        g_vkeys[2].key = VKEY_LEFT;

        g_vkeys[3].x = cx + hs;  g_vkeys[3].y = cy - hs;
        g_vkeys[3].w = ds;       g_vkeys[3].h = ds;
        g_vkeys[3].key = VKEY_RIGHT;

        /* 动作键 A (主操作) - 右下 */
        g_vkeys[4].x = g_screen_w - margin - 2 * as - gap;
        g_vkeys[4].y = g_screen_h - margin - as;
        g_vkeys[4].w = as;       g_vkeys[4].h = as;
        g_vkeys[4].key = VKEY_A;

        /* 动作键 B (副操作) - 右下 */
        g_vkeys[5].x = g_screen_w - margin - as;
        g_vkeys[5].y = g_screen_h - margin - as;
        g_vkeys[5].w = as;       g_vkeys[5].h = as;
        g_vkeys[5].key = VKEY_B;

        g_vkey_count = 6;
        break;
    }

    case VLAYOUT_HORIZONTAL: {
        /* 左右方向键放在屏幕左右两侧底部 (避开中央按钮栏, 高度限制不超出底栏) */
        int hkey = ds;
        if (hkey > 56) hkey = 56;  /* 限制高度适配 80px 底部栏 */
        int sy = g_screen_h - margin - hkey - br;

        g_vkeys[0].x = margin;
        g_vkeys[0].y = sy;
        g_vkeys[0].w = hkey;
        g_vkeys[0].h = hkey;
        g_vkeys[0].key = VKEY_LEFT;

        g_vkeys[1].x = g_screen_w - margin - hkey;
        g_vkeys[1].y = sy;
        g_vkeys[1].w = hkey;
        g_vkeys[1].h = hkey;
        g_vkeys[1].key = VKEY_RIGHT;

        g_vkey_count = 2;
        break;
    }
    }
}

/* 绘制所有虚拟键 */
void touch_ctrl_draw(void)
{
    for (int i = 0; i < g_vkey_count; i++) {
        vkey_rect_t *vk = &g_vkeys[i];
        unsigned int fill   = 0x66333333u;  /* 半透明深灰 */
        unsigned int arrow  = 0xCCFFFFFFu;  /* 亮白箭头 */

        switch (vk->key) {
        case VKEY_UP:
        case VKEY_DOWN:
        case VKEY_LEFT:
        case VKEY_RIGHT:
            draw_dpad_button(vk->x, vk->y, vk->w, vk->h,
                             vk->key, fill, arrow);
            break;
        case VKEY_A:
            draw_action_button(vk->x + vk->w / 2, vk->y + vk->h / 2,
                               vk->w / 2, "A", 0x664CAF50u, 0xCCFFFFFFu);
            break;
        case VKEY_B:
            draw_action_button(vk->x + vk->w / 2, vk->y + vk->h / 2,
                               vk->w / 2, "B", 0x66F44336u, 0xCCFFFFFFu);
            break;
        default:
            break;
        }
    }
}

/* 命中检测 */
vkey_t touch_ctrl_hit(int x, int y)
{
    for (int i = 0; i < g_vkey_count; i++) {
        vkey_rect_t *vk = &g_vkeys[i];
        if (x >= vk->x && x < vk->x + vk->w &&
            y >= vk->y && y < vk->y + vk->h) {
            return vk->key;
        }
    }
    return VKEY_COUNT;
}

/* 擦除虚拟按键区域 */
void touch_ctrl_erase(unsigned int bg)
{
    for (int i = 0; i < g_vkey_count; i++) {
        vkey_rect_t *vk = &g_vkeys[i];
        /* 略扩大擦除范围以确保清理干净 (含箭头溢出) */
        int margin = 4;
        ui_fill_rect(vk->x - margin, vk->y - margin,
                     vk->w + 2 * margin, vk->h + 2 * margin, bg);
    }
}
