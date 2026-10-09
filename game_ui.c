#include "game_ui.h"
#include "app_ui.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define C_BG       APP_COLOR_BG
#define C_HEADER   APP_COLOR_HEADER
#define C_PANEL    APP_COLOR_PANEL
#define C_CARD     APP_COLOR_SURFACE
#define C_BORDER   APP_COLOR_BORDER
#define C_ACCENT   APP_COLOR_PRIMARY
#define C_MUTED    APP_COLOR_MUTED
#define C_DANGER   APP_COLOR_DANGER

static int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void draw_button(int x, int y, int w, int h, const char *label,
                        unsigned int fill)
{
    ui_button_t b;
    b.x = x; b.y = y; b.w = w; b.h = h; b.label = label; b.id = 0;
    ui_draw_button(&b, fill, C_BORDER, UI_COLOR_WHITE, C_HEADER, 2);
}

void game_ui_layout(game_ui_layout_t *l)
{
    int margin = 16;
    int gap = 16;
    int panel_w = g_screen_w >= 1100 ? 288 : 240;

    l->header_h = 64;
    l->panel_x = g_screen_w - margin - panel_w;
    l->panel_y = l->header_h + margin;
    l->panel_w = panel_w;
    l->panel_h = g_screen_h - l->panel_y - margin;

    l->play_x = margin;
    l->play_y = l->panel_y;
    l->play_w = l->panel_x - gap - l->play_x;
    l->play_h = l->panel_h;

    l->dpad_size = l->panel_w >= 280 ? 68 : 58;
    l->dpad_x = l->panel_x + (l->panel_w - l->dpad_size * 3) / 2;
    l->dpad_y = l->panel_y + 94;

    l->pause_x = l->panel_x + 16;
    l->pause_y = l->panel_y + l->panel_h - 124;
    l->pause_w = l->panel_w - 32;
    l->pause_h = 48;
    l->back_x = l->pause_x;
    l->back_y = l->pause_y + 60;
    l->back_w = l->pause_w;
    l->back_h = 48;
}

void game_ui_draw_score(const game_ui_layout_t *l, int score, int best)
{
    char text[64];
    int x = g_screen_w - 340;
    (void)l;
    ui_fill_rect(x, 0, 340, 64, C_HEADER);
    snprintf(text, sizeof(text), "SCORE %04d", score);
    ui_draw_text(x + 12, 20, text, C_ACCENT, C_HEADER, 2);
    if (best >= 0) {
        snprintf(text, sizeof(text), "BEST %04d", best);
        ui_draw_text(x + 182, 20, text, UI_COLOR_WHITE, C_HEADER, 2);
    }
}

void game_ui_draw_shell(const game_ui_layout_t *l,
                        const char *title,
                        int score,
                        int best,
                        game_difficulty_t difficulty,
                        const char *help_line)
{
    static const char *diff[] = { "EASY", "NORMAL", "HARD" };
    ui_clear(C_BG);
    ui_fill_rect(0, 0, g_screen_w, l->header_h, C_HEADER);
    ui_fill_rect(0, l->header_h - 2, g_screen_w, 2, C_ACCENT);
    ui_draw_text(22, 16, title, UI_COLOR_WHITE, C_HEADER, 3);
    game_ui_draw_score(l, score, best);

    ui_fill_rect(l->play_x, l->play_y, l->play_w, l->play_h, 0xFF0B111Au);
    ui_draw_rect(l->play_x, l->play_y, l->play_w, l->play_h, C_BORDER);
    ui_fill_rect(l->panel_x, l->panel_y, l->panel_w, l->panel_h, C_PANEL);
    ui_draw_rect(l->panel_x, l->panel_y, l->panel_w, l->panel_h, C_BORDER);

    ui_draw_text(l->panel_x + 16, l->panel_y + 16,
                 diff[(int)difficulty], C_ACCENT, C_PANEL, 2);
    if (help_line)
        ui_draw_text(l->panel_x + 16, l->panel_y + 50,
                     help_line, C_MUTED, C_PANEL, 1);

    draw_button(l->pause_x, l->pause_y, l->pause_w, l->pause_h,
                "PAUSE", C_CARD);
    draw_button(l->back_x, l->back_y, l->back_w, l->back_h,
                "BACK", 0xFF633044u);
}

static void draw_arrow(int cx, int cy, game_ui_action_t action)
{
    int d;
    for (d = 0; d < 13; d++) {
        int span = 12 - d;
        if (action == GAME_UI_UP)
            ui_draw_hline(cx - span, cy + 7 - d, span * 2 + 1, UI_COLOR_WHITE);
        else if (action == GAME_UI_DOWN)
            ui_draw_hline(cx - span, cy - 7 + d, span * 2 + 1, UI_COLOR_WHITE);
        else if (action == GAME_UI_LEFT)
            ui_draw_vline(cx + 7 - d, cy - span, span * 2 + 1, UI_COLOR_WHITE);
        else
            ui_draw_vline(cx - 7 + d, cy - span, span * 2 + 1, UI_COLOR_WHITE);
    }
}

void game_ui_draw_dpad(const game_ui_layout_t *l)
{
    const int s = l->dpad_size;
    const int x = l->dpad_x;
    const int y = l->dpad_y;
    const int pos[4][2] = { {1, 0}, {1, 2}, {0, 1}, {2, 1} };
    const game_ui_action_t act[4] = {
        GAME_UI_UP, GAME_UI_DOWN, GAME_UI_LEFT, GAME_UI_RIGHT
    };
    int i;

    for (i = 0; i < 4; i++) {
        int bx = x + pos[i][0] * s;
        int by = y + pos[i][1] * s;
        ui_fill_rect(bx + 2, by, s - 4, s, C_CARD);
        ui_fill_rect(bx, by + 2, s, s - 4, C_CARD);
        ui_draw_rect(bx, by, s, s, C_BORDER);
        draw_arrow(bx + s / 2, by + s / 2, act[i]);
    }
    ui_fill_circle(x + s + s / 2, y + s + s / 2, 7, C_ACCENT);
}

game_ui_action_t game_ui_hit(const game_ui_layout_t *l, int x, int y)
{
    int s = l->dpad_size;
    if (in_rect(x, y, l->pause_x, l->pause_y, l->pause_w, l->pause_h))
        return GAME_UI_PAUSE;
    if (in_rect(x, y, l->back_x, l->back_y, l->back_w, l->back_h))
        return GAME_UI_BACK;
    if (in_rect(x, y, l->dpad_x + s, l->dpad_y, s, s))
        return GAME_UI_UP;
    if (in_rect(x, y, l->dpad_x + s, l->dpad_y + 2 * s, s, s))
        return GAME_UI_DOWN;
    if (in_rect(x, y, l->dpad_x, l->dpad_y + s, s, s))
        return GAME_UI_LEFT;
    if (in_rect(x, y, l->dpad_x + 2 * s, l->dpad_y + s, s, s))
        return GAME_UI_RIGHT;
    return GAME_UI_NONE;
}

void game_ui_draw_overlay(const game_ui_layout_t *l,
                          const char *title,
                          const char *subtitle,
                          int show_restart)
{
    int w = l->play_w * 2 / 3;
    int h = 190;
    int x = l->play_x + (l->play_w - w) / 2;
    int y = l->play_y + (l->play_h - h) / 2;
    ui_fill_rect(x, y, w, h, C_HEADER);
    ui_draw_rect(x, y, w, h, C_ACCENT);
    ui_draw_text_center(x + w / 2, y + 38, title,
                        show_restart ? C_DANGER : C_ACCENT, C_HEADER, 3);
    ui_draw_text_center(x + w / 2, y + 94, subtitle,
                        C_MUTED, C_HEADER, 1);
    ui_draw_text_center(x + w / 2, y + 132,
                        show_restart ? "TAP PLAYFIELD TO RESTART" :
                                       "TAP PAUSE TO CONTINUE",
                        UI_COLOR_WHITE, C_HEADER, 2);
}
