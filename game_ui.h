#ifndef GAME_UI_H
#define GAME_UI_H

#include "game_menu.h"

typedef enum {
    GAME_UI_NONE = 0,
    GAME_UI_UP,
    GAME_UI_DOWN,
    GAME_UI_LEFT,
    GAME_UI_RIGHT,
    GAME_UI_PAUSE,
    GAME_UI_BACK,
    GAME_UI_RESTART
} game_ui_action_t;

typedef struct {
    int header_h;
    int play_x, play_y, play_w, play_h;
    int panel_x, panel_y, panel_w, panel_h;
    int dpad_x, dpad_y, dpad_size;
    int pause_x, pause_y, pause_w, pause_h;
    int back_x, back_y, back_w, back_h;
} game_ui_layout_t;

void game_ui_layout(game_ui_layout_t *layout);
void game_ui_draw_shell(const game_ui_layout_t *layout,
                        const char *title,
                        int score,
                        int best,
                        game_difficulty_t difficulty,
                        const char *help_line);
void game_ui_draw_score(const game_ui_layout_t *layout,
                        int score,
                        int best);
void game_ui_draw_dpad(const game_ui_layout_t *layout);
game_ui_action_t game_ui_hit(const game_ui_layout_t *layout, int x, int y);
void game_ui_draw_overlay(const game_ui_layout_t *layout,
                          const char *title,
                          const char *subtitle,
                          int show_restart);

#endif
