/* System launcher: one-screen information architecture for touch displays. */
#include "main_ui.h"
#include "app_ui.h"
#include "input.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define HEADER_H      72
#define CONTENT_PAD   30
#define GRID_GAP      14
#define UTILITY_GAP   22
#define TARGET_COUNT   8
#define PRIMARY_COUNT  5

typedef struct {
    const char *label;
    const char *detail;
    int id;
    app_icon_t icon;
    unsigned int accent;
    app_rect_t rect;
    int utility;
} launcher_item_t;

static launcher_item_t g_items[TARGET_COUNT] = {
    { "Album",    "Browse photos",     MAIN_BTN_ALBUM,
      APP_ICON_ALBUM, APP_COLOR_PRIMARY, {0}, 0 },
    { "Paint",    "Draw freely",       MAIN_BTN_PAINT,
      APP_ICON_PAINT, APP_COLOR_PRIMARY, {0}, 0 },
    { "Bounce",   "Arcade paddle",     MAIN_BTN_BALL,
      APP_ICON_BOUNCE, APP_COLOR_AMBER, {0}, 0 },
    { "Snake",    "Classic challenge", MAIN_BTN_SNAKE,
      APP_ICON_SNAKE, APP_COLOR_PRIMARY, {0}, 0 },
    { "Gomoku",   "Five in a row",     MAIN_BTN_GOMOKU,
      APP_ICON_GOMOKU, APP_COLOR_BLUE, {0}, 0 },
    { "Security", "Update password",   MAIN_BTN_SET_PASS,
      APP_ICON_SECURITY, APP_COLOR_PRIMARY, {0}, 1 },
    { "About",    "System details",    MAIN_BTN_ABOUT,
      APP_ICON_ABOUT, APP_COLOR_PRIMARY, {0}, 1 },
    { "Power",    "Shut down",         MAIN_BTN_POWER,
      APP_ICON_POWER, APP_COLOR_DANGER, {0}, 1 }
};

static int g_focus;

static void format_time_now(char *tbuf, size_t tbsz,
                            char *dbuf, size_t dbsz)
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm) {
        snprintf(tbuf, tbsz, "--:--:--");
        snprintf(dbuf, dbsz, "----/--/--");
        return;
    }
    strftime(tbuf, tbsz, "%H:%M:%S", tm);
    strftime(dbuf, dbsz, "%Y-%m-%d", tm);
}

static void layout_items(void)
{
    int utility_w = g_screen_w >= 1000 ? 260 : g_screen_w / 4;
    int utility_x = g_screen_w - CONTENT_PAD - utility_w;
    int grid_x = CONTENT_PAD;
    int grid_y = HEADER_H + 102;
    int grid_w = utility_x - UTILITY_GAP - grid_x;
    int grid_h = g_screen_h - grid_y - CONTENT_PAD;
    int card_w = (grid_w - GRID_GAP * 2) / 3;
    int card_h = (grid_h - GRID_GAP) / 2;
    int utility_y = HEADER_H + 76;
    int utility_h = g_screen_h - utility_y - CONTENT_PAD;
    int row_h = utility_h / 3;
    int i;

    for (i = 0; i < PRIMARY_COUNT; i++) {
        int row = i / 3;
        int col = i % 3;
        g_items[i].rect.x = grid_x + col * (card_w + GRID_GAP);
        g_items[i].rect.y = grid_y + row * (card_h + GRID_GAP);
        g_items[i].rect.w = card_w;
        g_items[i].rect.h = card_h;
    }
    for (i = PRIMARY_COUNT; i < TARGET_COUNT; i++) {
        int row = i - PRIMARY_COUNT;
        g_items[i].rect.x = utility_x;
        g_items[i].rect.y = utility_y + row * row_h;
        g_items[i].rect.w = utility_w;
        g_items[i].rect.h = row_h;
    }
}

int main_ui_draw_statusbar(void)
{
    char tbuf[32], dbuf[32];
    int time_w;
    int date_w;

    format_time_now(tbuf, sizeof(tbuf), dbuf, sizeof(dbuf));
    ui_fill_rect(0, 0, g_screen_w, HEADER_H, APP_COLOR_HEADER);
    ui_fill_rect(0, HEADER_H - 2, g_screen_w, 2, APP_COLOR_BORDER);
    ui_draw_text(24, 19, "RK SYSTEM", UI_COLOR_WHITE,
                 APP_COLOR_HEADER, 2);

    time_w = (int)strlen(tbuf) * 8 * 2;
    date_w = (int)strlen(dbuf) * 8;
    ui_draw_text(g_screen_w - 24 - time_w, 14, tbuf,
                 UI_COLOR_WHITE, APP_COLOR_HEADER, 2);
    ui_draw_text(g_screen_w - 24 - date_w, 44, dbuf,
                 APP_COLOR_MUTED, APP_COLOR_HEADER, 1);
    return 0;
}

static void draw_primary(const launcher_item_t *item, int focused,
                         int pressed)
{
    const app_rect_t *r = &item->rect;
    unsigned int fill = pressed ? APP_COLOR_SURFACE_2 : APP_COLOR_SURFACE;
    unsigned int border = focused ? item->accent : APP_COLOR_BORDER;
    int short_side = r->w < r->h ? r->w : r->h;
    int icon_size = short_side / 3;
    int icon_y = r->y + r->h / 2 - 26;
    int label_scale = r->w >= 180 ? 2 : 1;

    app_draw_surface(r, fill, border, 7);
    if (focused)
        ui_fill_rect(r->x, r->y + 7, 4, r->h - 14, item->accent);
    app_draw_icon(item->icon, r->x + r->w / 2, icon_y,
                  icon_size, item->accent);
    ui_draw_text_center(r->x + r->w / 2, r->y + r->h - 48,
                        item->label, UI_COLOR_WHITE, fill, label_scale);
    ui_draw_text_center(r->x + r->w / 2, r->y + r->h - 20,
                        item->detail, APP_COLOR_MUTED, fill, 1);
}

static void draw_utility(const launcher_item_t *item, int focused,
                         int pressed, int position)
{
    const app_rect_t *r = &item->rect;
    unsigned int fill = pressed ? APP_COLOR_SURFACE_2 : APP_COLOR_SURFACE;
    unsigned int border = focused ? item->accent : APP_COLOR_BORDER;
    int compact = r->w < 240;
    int icon_size = compact ? 32 : (r->h > 100 ? 46 : 36);
    int icon_x = r->x + icon_size / 2 + 18;
    int text_x = r->x + icon_size + (compact ? 26 : 34);
    int label_y = r->y + r->h / 2 - 24;

    ui_fill_rect(r->x, r->y, r->w, r->h, fill);
    if (position == 0)
        ui_draw_round_rect(r->x, r->y, r->w, r->h, 7, border);
    else
        ui_draw_rect(r->x, r->y, r->w, r->h, border);
    if (focused) ui_fill_rect(r->x, r->y, 5, r->h, item->accent);
    app_draw_icon(item->icon, icon_x, r->y + r->h / 2,
                  icon_size, item->accent);
    ui_draw_text(text_x, label_y, item->label,
                 UI_COLOR_WHITE, fill, compact ? 1 : 2);
    ui_draw_text(text_x, label_y + 38, item->detail,
                 APP_COLOR_MUTED, fill, 1);
    app_draw_chevron(r->x + r->w - 20, r->y + r->h / 2,
                     20, item->accent);
}

static void draw_item(int index, int pressed)
{
    if (index < 0 || index >= TARGET_COUNT) return;
    if (g_items[index].utility)
        draw_utility(&g_items[index], index == g_focus, pressed,
                     index - PRIMARY_COUNT);
    else
        draw_primary(&g_items[index], index == g_focus, pressed);
}

int main_ui_draw(void)
{
    int i;
    ui_clear(APP_COLOR_BG);
    layout_items();
    main_ui_draw_statusbar();
    ui_draw_text(CONTENT_PAD, HEADER_H + 18, "HOME",
                 APP_COLOR_PRIMARY, APP_COLOR_BG, 3);
    ui_draw_text(CONTENT_PAD, HEADER_H + 70,
                 "Choose an app or swipe to move focus",
                 APP_COLOR_MUTED, APP_COLOR_BG, 1);
    for (i = 0; i < TARGET_COUNT; i++) draw_item(i, 0);
    return 0;
}

static void move_focus(int delta)
{
    int previous = g_focus;
    g_focus = (g_focus + delta + TARGET_COUNT) % TARGET_COUNT;
    draw_item(previous, 0);
    draw_item(g_focus, 0);
}

int main_ui_run(void)
{
    time_t last_time = 0;
    main_ui_draw();

    for (;;) {
        input_event_t ev;
        int i;
        int r = input_wait_event(&ev, 500);
        time_t now = time(NULL);

        if (now != last_time) {
            main_ui_draw_statusbar();
            last_time = now;
        }
        if (r < 0) continue;
        if (ev.type == INPUT_QUIT) return MAIN_BTN_NONE;
        if (ev.type == INPUT_KEY_POWER) return MAIN_BTN_POWER;
        if (ev.type == INPUT_SWIPE_LEFT ||
            ev.type == INPUT_SWIPE_DOWN) {
            move_focus(1);
            continue;
        }
        if (ev.type == INPUT_SWIPE_RIGHT ||
            ev.type == INPUT_SWIPE_UP) {
            move_focus(-1);
            continue;
        }
        if (ev.type != INPUT_TAP) continue;

        for (i = 0; i < TARGET_COUNT; i++) {
            if (!app_rect_hit(&g_items[i].rect, ev.x, ev.y)) continue;
            if (g_focus != i) {
                int previous = g_focus;
                g_focus = i;
                draw_item(previous, 0);
            }
            draw_item(i, 1);
            usleep(90 * 1000);
            return g_items[i].id;
        }
    }
}
