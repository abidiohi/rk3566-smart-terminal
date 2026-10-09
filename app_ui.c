#include "app_ui.h"

static void thick_line(int x0, int y0, int x1, int y1, int width,
                       unsigned int color)
{
    int i;
    int half = width / 2;
    for (i = -half; i <= half; i++) {
        if (x0 == x1)
            ui_draw_line(x0 + i, y0, x1 + i, y1, color);
        else
            ui_draw_line(x0, y0 + i, x1, y1 + i, color);
    }
}

int app_rect_hit(const app_rect_t *r, int x, int y)
{
    return r && x >= r->x && x < r->x + r->w &&
           y >= r->y && y < r->y + r->h;
}

void app_draw_surface(const app_rect_t *r, unsigned int fill,
                      unsigned int border, int radius)
{
    if (!r) return;
    ui_fill_round_rect(r->x, r->y, r->w, r->h, radius, fill);
    ui_draw_round_rect(r->x, r->y, r->w, r->h, radius, border);
}

void app_draw_chevron(int cx, int cy, int size, unsigned int color)
{
    ui_draw_line(cx - size / 3, cy - size / 2, cx + size / 3, cy, color);
    ui_draw_line(cx + size / 3, cy, cx - size / 3, cy + size / 2, color);
}

void app_draw_icon(app_icon_t icon, int cx, int cy, int size,
                   unsigned int color)
{
    int s = size;
    int q;
    int i;

    if (s < 20) s = 20;
    q = s / 4;
    switch (icon) {
    case APP_ICON_ALBUM:
        ui_draw_round_rect(cx - s / 2, cy - s / 2, s, s * 3 / 4,
                           4, color);
        ui_fill_circle(cx + s / 5, cy - s / 5, s / 12, color);
        thick_line(cx - s / 2 + 5, cy + s / 8,
                   cx - s / 6, cy - s / 10, 3, color);
        thick_line(cx - s / 6, cy - s / 10,
                   cx + s / 10, cy + s / 7, 3, color);
        thick_line(cx + s / 10, cy + s / 7,
                   cx + s / 3, cy - s / 12, 3, color);
        break;
    case APP_ICON_PAINT:
        thick_line(cx - s / 4, cy + s / 4,
                   cx + s / 3, cy - s / 3, 7, color);
        ui_fill_circle(cx - s / 3, cy + s / 3, s / 7, color);
        ui_fill_circle(cx - s / 2 + 2, cy + s / 2 - 2, s / 10, color);
        break;
    case APP_ICON_BOUNCE:
        ui_fill_round_rect(cx - s / 2, cy + s / 3,
                           s * 2 / 3, s / 8, 3, color);
        ui_draw_circle(cx + s / 3, cy - s / 3, s / 7, color);
        for (i = 0; i < 4; i++)
            ui_draw_line(cx - s / 7 + i * s / 9, cy + s / 5 - i * s / 7,
                         cx - s / 10 + i * s / 9,
                         cy + s / 8 - i * s / 7, color);
        break;
    case APP_ICON_SNAKE:
        for (i = 0; i < 5; i++)
            ui_fill_round_rect(cx - s / 2 + i * q / 2,
                               cy + s / 4 - (i > 2 ? (i - 2) * q / 2 : 0),
                               q, q, 3, color);
        ui_fill_round_rect(cx, cy - s / 3, q + 4, q, 4, color);
        ui_fill_circle(cx + q - 3, cy - s / 3 + q / 3, 2, APP_COLOR_BG);
        break;
    case APP_ICON_GOMOKU:
        for (i = -1; i <= 1; i++) {
            ui_draw_hline(cx - s / 2, cy + i * s / 4, s, color);
            ui_draw_vline(cx + i * s / 4, cy - s / 2, s, color);
        }
        ui_fill_circle(cx - s / 4, cy, s / 10, UI_COLOR_WHITE);
        ui_fill_circle(cx, cy + s / 4, s / 10, color);
        ui_fill_circle(cx + s / 4, cy - s / 4, s / 10, UI_COLOR_WHITE);
        break;
    case APP_ICON_SECURITY:
        ui_draw_round_rect(cx - s / 3, cy - s / 12,
                           s * 2 / 3, s / 2, 4, color);
        ui_draw_circle(cx, cy - s / 12, s / 5, color);
        ui_fill_rect(cx - s / 5 - 2, cy - s / 12,
                     s * 2 / 5 + 4, s / 5, APP_COLOR_SURFACE);
        ui_fill_circle(cx, cy + s / 8, 3, color);
        break;
    case APP_ICON_ABOUT:
        ui_draw_circle(cx, cy, s / 2, color);
        ui_fill_circle(cx, cy - s / 5, 3, color);
        ui_fill_rect(cx - 2, cy - 2, 5, s / 4, color);
        ui_fill_rect(cx - s / 10, cy + s / 4 - 3, s / 5, 4, color);
        break;
    case APP_ICON_POWER:
        ui_draw_circle(cx, cy + 2, s / 2, color);
        ui_fill_rect(cx - s / 3, cy - s / 2 - 2, s * 2 / 3, s / 3,
                     APP_COLOR_SURFACE);
        thick_line(cx, cy - s / 2, cx, cy, 4, color);
        break;
    }
}
