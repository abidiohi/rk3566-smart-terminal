#include "snake.h"
#include "game_menu.h"
#include "game_ui.h"
#include "input.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_SNAKE_LEN 1024
#define MIN_STEP_MS   55

typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } direction_t;

typedef struct {
    game_ui_layout_t ui;
    int cell;
    int board_x, board_y, cols, rows;
    int body_x[MAX_SNAKE_LEN];
    int body_y[MAX_SNAKE_LEN];
    int length;
    int food_x, food_y;
    direction_t direction;
    direction_t queued_direction;
    int score;
    int best;
    int step_ms;
    int speed_up;
    int game_over;
    int paused;
} snake_game_t;

static long monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static int is_opposite(direction_t a, direction_t b)
{
    return (a == DIR_UP && b == DIR_DOWN) ||
           (a == DIR_DOWN && b == DIR_UP) ||
           (a == DIR_LEFT && b == DIR_RIGHT) ||
           (a == DIR_RIGHT && b == DIR_LEFT);
}

static int snake_occupies(const snake_game_t *g, int x, int y, int count)
{
    int i;
    for (i = 0; i < count; i++)
        if (g->body_x[i] == x && g->body_y[i] == y)
            return 1;
    return 0;
}

static void draw_board_cell(const snake_game_t *g, int x, int y,
                            unsigned int color)
{
    int px = g->board_x + x * g->cell;
    int py = g->board_y + y * g->cell;
    ui_fill_rect(px + 1, py + 1, g->cell - 2, g->cell - 2, color);
}

static void draw_empty_cell(const snake_game_t *g, int x, int y)
{
    draw_board_cell(g, x, y, 0xFF142130u);
}

static void draw_food(const snake_game_t *g)
{
    int cx = g->board_x + g->food_x * g->cell + g->cell / 2;
    int cy = g->board_y + g->food_y * g->cell + g->cell / 2;
    ui_fill_circle(cx, cy, g->cell / 3, 0xFFFFC857u);
    ui_fill_circle(cx - 2, cy - 2, g->cell / 7, UI_COLOR_WHITE);
}

static void draw_snake_cell(const snake_game_t *g, int index)
{
    unsigned int color = index == 0 ? 0xFF38F2A2u : 0xFF22A875u;
    draw_board_cell(g, g->body_x[index], g->body_y[index], color);
    if (index == 0) {
        int px = g->board_x + g->body_x[0] * g->cell;
        int py = g->board_y + g->body_y[0] * g->cell;
        ui_fill_circle(px + g->cell / 3, py + g->cell / 3,
                       g->cell / 10 + 1, 0xFF07110Cu);
        ui_fill_circle(px + g->cell * 2 / 3, py + g->cell / 3,
                       g->cell / 10 + 1, 0xFF07110Cu);
    }
}

static void place_food(snake_game_t *g)
{
    int tries;
    for (tries = 0; tries < g->cols * g->rows * 2; tries++) {
        int x = rand() % g->cols;
        int y = rand() % g->rows;
        if (!snake_occupies(g, x, y, g->length)) {
            g->food_x = x;
            g->food_y = y;
            return;
        }
    }
    for (g->food_y = 0; g->food_y < g->rows; g->food_y++)
        for (g->food_x = 0; g->food_x < g->cols; g->food_x++)
            if (!snake_occupies(g, g->food_x, g->food_y, g->length))
                return;
}

static void draw_board(snake_game_t *g)
{
    int x, y, i;
    ui_fill_rect(g->ui.play_x, g->ui.play_y,
                 g->ui.play_w, g->ui.play_h, 0xFF0B111Au);
    ui_fill_rect(g->board_x - 2, g->board_y - 2,
                 g->cols * g->cell + 4, g->rows * g->cell + 4,
                 0xFF2D465Fu);
    for (y = 0; y < g->rows; y++)
        for (x = 0; x < g->cols; x++)
            draw_empty_cell(g, x, y);
    draw_food(g);
    for (i = g->length - 1; i >= 0; i--)
        draw_snake_cell(g, i);
}

static void reset_game(snake_game_t *g, int step_ms)
{
    int usable_w, usable_h;
    g->cell = 20;
    usable_w = g->ui.play_w - 24;
    usable_h = g->ui.play_h - 24;
    g->cols = usable_w / g->cell;
    g->rows = usable_h / g->cell;
    if (g->cols * g->rows > MAX_SNAKE_LEN) {
        while (g->cols * g->rows > MAX_SNAKE_LEN)
            g->cols--;
    }
    g->board_x = g->ui.play_x + (g->ui.play_w - g->cols * g->cell) / 2;
    g->board_y = g->ui.play_y + (g->ui.play_h - g->rows * g->cell) / 2;

    g->length = 4;
    g->body_x[0] = g->cols / 2;
    g->body_y[0] = g->rows / 2;
    g->body_x[1] = g->body_x[0] - 1;
    g->body_y[1] = g->body_y[0];
    g->body_x[2] = g->body_x[0] - 2;
    g->body_y[2] = g->body_y[0];
    g->body_x[3] = g->body_x[0] - 3;
    g->body_y[3] = g->body_y[0];
    g->direction = DIR_RIGHT;
    g->queued_direction = DIR_RIGHT;
    g->score = 0;
    g->step_ms = step_ms;
    g->game_over = 0;
    g->paused = 0;
    place_food(g);
}

static void set_direction(snake_game_t *g, direction_t direction)
{
    if (!is_opposite(g->direction, direction))
        g->queued_direction = direction;
}

static void handle_action(snake_game_t *g, game_ui_action_t action)
{
    if (action == GAME_UI_UP) set_direction(g, DIR_UP);
    else if (action == GAME_UI_DOWN) set_direction(g, DIR_DOWN);
    else if (action == GAME_UI_LEFT) set_direction(g, DIR_LEFT);
    else if (action == GAME_UI_RIGHT) set_direction(g, DIR_RIGHT);
}

static void finish_game(snake_game_t *g)
{
    g->game_over = 1;
    if (g->score > g->best) g->best = g->score;
    game_ui_draw_score(&g->ui, g->score, g->best);
    game_ui_draw_overlay(&g->ui, "GAME OVER",
                         "Your run has ended", 1);
}

static void step_game(snake_game_t *g)
{
    int nx, ny, ate, check_count, i;
    int old_tail_x = g->body_x[g->length - 1];
    int old_tail_y = g->body_y[g->length - 1];

    g->direction = g->queued_direction;
    nx = g->body_x[0];
    ny = g->body_y[0];
    if (g->direction == DIR_UP) ny--;
    else if (g->direction == DIR_DOWN) ny++;
    else if (g->direction == DIR_LEFT) nx--;
    else nx++;

    if (nx < 0 || nx >= g->cols || ny < 0 || ny >= g->rows) {
        finish_game(g);
        return;
    }

    ate = nx == g->food_x && ny == g->food_y;
    check_count = ate ? g->length : g->length - 1;
    if (snake_occupies(g, nx, ny, check_count)) {
        finish_game(g);
        return;
    }

    if (ate && g->length < MAX_SNAKE_LEN)
        g->length++;
    for (i = g->length - 1; i > 0; i--) {
        g->body_x[i] = g->body_x[i - 1];
        g->body_y[i] = g->body_y[i - 1];
    }
    g->body_x[0] = nx;
    g->body_y[0] = ny;

    if (!ate)
        draw_empty_cell(g, old_tail_x, old_tail_y);
    if (g->length > 1)
        draw_snake_cell(g, 1);
    draw_snake_cell(g, 0);

    if (ate) {
        g->score += 10;
        if (g->score > g->best) g->best = g->score;
        if (g->step_ms > MIN_STEP_MS + g->speed_up)
            g->step_ms -= g->speed_up;
        place_food(g);
        draw_food(g);
        game_ui_draw_score(&g->ui, g->score, g->best);
    }
}

static int snake_game_loop(game_menu_ctx_t *ctx, int *best)
{
    snake_game_t g;
    long next_step;
    int start_step = ctx->difficulty == GAME_DIFF_EASY ? 210 :
                     ctx->difficulty == GAME_DIFF_HARD ? 105 : 155;
    memset(&g, 0, sizeof(g));
    game_ui_layout(&g.ui);
    g.best = *best;
    g.speed_up = ctx->settings_count ? ctx->settings[0].value : 5;
    reset_game(&g, start_step);

    game_ui_draw_shell(&g.ui, "SNAKE", g.score, g.best,
                       ctx->difficulty, "SWIPE OR USE D-PAD");
    game_ui_draw_dpad(&g.ui);
    draw_board(&g);
    next_step = monotonic_ms() + g.step_ms;

    for (;;) {
        input_event_t ev;
        int r = input_poll(&ev);
        if (r == 0 && ev.type != INPUT_NONE) {
            game_ui_action_t action = GAME_UI_NONE;
            if (ev.type == INPUT_QUIT) return -1;
            if (ev.type == INPUT_KEY_BACK) break;
            if (ev.type == INPUT_TAP)
                action = game_ui_hit(&g.ui, ev.x, ev.y);

            if (action == GAME_UI_BACK) break;
            if (action == GAME_UI_PAUSE && !g.game_over) {
                g.paused = !g.paused;
                if (g.paused)
                    game_ui_draw_overlay(&g.ui, "PAUSED",
                                         "Game clock stopped", 0);
                else {
                    draw_board(&g);
                    next_step = monotonic_ms() + g.step_ms;
                }
                continue;
            }
            if (g.game_over) {
                if (ev.type == INPUT_TAP &&
                    ev.x >= g.ui.play_x && ev.x < g.ui.play_x + g.ui.play_w &&
                    ev.y >= g.ui.play_y && ev.y < g.ui.play_y + g.ui.play_h) {
                    game_menu_add_score(ctx, g.score);
                    reset_game(&g, start_step);
                    game_ui_draw_score(&g.ui, g.score, g.best);
                    draw_board(&g);
                    next_step = monotonic_ms() + g.step_ms;
                }
                continue;
            }
            if (!g.paused) {
                if (ev.type == INPUT_SWIPE_UP) set_direction(&g, DIR_UP);
                else if (ev.type == INPUT_SWIPE_DOWN) set_direction(&g, DIR_DOWN);
                else if (ev.type == INPUT_SWIPE_LEFT) set_direction(&g, DIR_LEFT);
                else if (ev.type == INPUT_SWIPE_RIGHT) set_direction(&g, DIR_RIGHT);
                else handle_action(&g, action);
            }
        }

        if (!g.paused && !g.game_over && monotonic_ms() >= next_step) {
            step_game(&g);
            next_step += g.step_ms;
            if (next_step < monotonic_ms())
                next_step = monotonic_ms() + g.step_ms;
        }
        usleep(4000);
    }

    game_menu_add_score(ctx, g.score);
    *best = g.best;
    return 0;
}

int snake_run(void)
{
    game_menu_ctx_t ctx;
    int best = 0;
    game_menu_init(&ctx);
    ctx.title = "SNAKE";
    ctx.title_color = 0xFF22A875u;
    ctx.settings[0].name = "SPEED STEP";
    ctx.settings[0].value = 5;
    ctx.settings[0].min_val = 1;
    ctx.settings[0].max_val = 10;
    ctx.settings[0].step = 1;
    ctx.settings_count = 1;
    srand((unsigned)time(NULL));

    for (;;) {
        gmenu_result_t result = game_menu_run(&ctx);
        if (result == GMENU_EXIT) return 0;
        if (result == GMENU_START_GAME &&
            snake_game_loop(&ctx, &best) < 0)
            return -1;
    }
}
