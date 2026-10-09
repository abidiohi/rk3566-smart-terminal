#include "ball.h"
#include "game_menu.h"
#include "game_ui.h"
#include "input.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define FRAME_MS 20

typedef struct {
    game_ui_layout_t ui;
    int field_x, field_y, field_w, field_h;
    int ball_x, ball_y, ball_r;
    int velocity_x, velocity_y;
    int paddle_x, paddle_y, paddle_w, paddle_h;
    int score, best;
    int base_speed;
    int game_over;
    int paused;
    int touching;
    int held_direction;
} ball_game_t;

static long monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

static void clamp_paddle(ball_game_t *g)
{
    if (g->paddle_x < g->field_x + 2)
        g->paddle_x = g->field_x + 2;
    if (g->paddle_x + g->paddle_w > g->field_x + g->field_w - 2)
        g->paddle_x = g->field_x + g->field_w - g->paddle_w - 2;
}

static void draw_field(const ball_game_t *g)
{
    ui_fill_rect(g->field_x, g->field_y, g->field_w, g->field_h,
                 0xFF0B1420u);
    ui_draw_rect(g->field_x, g->field_y, g->field_w, g->field_h,
                 0xFF3A5068u);
}

static void fill_field_rect(const ball_game_t *g, int x, int y, int w, int h)
{
    int x2 = x + w;
    int y2 = y + h;
    if (x < g->field_x + 1) x = g->field_x + 1;
    if (y < g->field_y + 1) y = g->field_y + 1;
    if (x2 > g->field_x + g->field_w - 1)
        x2 = g->field_x + g->field_w - 1;
    if (y2 > g->field_y + g->field_h - 1)
        y2 = g->field_y + g->field_h - 1;
    if (x2 > x && y2 > y)
        ui_fill_rect(x, y, x2 - x, y2 - y, 0xFF0B1420u);
}

static void erase_ball(const ball_game_t *g, int x, int y)
{
    fill_field_rect(g, x - g->ball_r - 2, y - g->ball_r - 2,
                    g->ball_r * 2 + 5, g->ball_r * 2 + 5);
}

static void draw_ball(const ball_game_t *g)
{
    ui_fill_circle(g->ball_x, g->ball_y, g->ball_r, 0xFFFFC857u);
    ui_fill_circle(g->ball_x - 3, g->ball_y - 3,
                   g->ball_r / 3, UI_COLOR_WHITE);
}

static void erase_paddle(const ball_game_t *g, int x)
{
    fill_field_rect(g, x - 2, g->paddle_y - 2,
                    g->paddle_w + 4, g->paddle_h + 4);
}

static void draw_paddle(const ball_game_t *g)
{
    ui_fill_rect(g->paddle_x, g->paddle_y,
                 g->paddle_w, g->paddle_h, 0xFF3DDC97u);
    ui_fill_rect(g->paddle_x + 10, g->paddle_y + 4,
                 g->paddle_w - 20, 3, 0xFFB7FFE1u);
}

static void draw_dynamic(const ball_game_t *g)
{
    draw_paddle(g);
    if (!g->game_over) draw_ball(g);
}

static void reset_game(ball_game_t *g, int base_speed, int paddle_w)
{
    g->field_x = g->ui.play_x + 8;
    g->field_y = g->ui.play_y + 8;
    g->field_w = g->ui.play_w - 16;
    g->field_h = g->ui.play_h - 16;
    g->ball_r = 11;
    g->paddle_w = paddle_w;
    g->paddle_h = 18;
    g->paddle_x = g->field_x + (g->field_w - g->paddle_w) / 2;
    g->paddle_y = g->field_y + g->field_h - 36;
    g->ball_x = g->field_x + g->field_w / 2;
    g->ball_y = g->field_y + g->field_h / 3;
    g->base_speed = base_speed;
    g->velocity_x = (rand() & 1) ? base_speed : -base_speed;
    g->velocity_y = base_speed;
    g->score = 0;
    g->game_over = 0;
    g->paused = 0;
    g->touching = 0;
    g->held_direction = 0;
}

static void finish_game(ball_game_t *g)
{
    g->game_over = 1;
    if (g->score > g->best) g->best = g->score;
    game_ui_draw_score(&g->ui, g->score, g->best);
    game_ui_draw_overlay(&g->ui, "GAME OVER",
                         "The ball slipped past", 1);
}

static void update_physics(ball_game_t *g)
{
    int old_x = g->ball_x;
    int old_y = g->ball_y;
    int old_paddle = g->paddle_x;
    int next_y;

    if (g->held_direction < 0) g->paddle_x -= 9;
    if (g->held_direction > 0) g->paddle_x += 9;
    clamp_paddle(g);

    g->ball_x += g->velocity_x;
    g->ball_y += g->velocity_y;

    if (g->ball_x - g->ball_r <= g->field_x) {
        g->ball_x = g->field_x + g->ball_r;
        g->velocity_x = abs(g->velocity_x);
    } else if (g->ball_x + g->ball_r >= g->field_x + g->field_w) {
        g->ball_x = g->field_x + g->field_w - g->ball_r;
        g->velocity_x = -abs(g->velocity_x);
    }
    if (g->ball_y - g->ball_r <= g->field_y) {
        g->ball_y = g->field_y + g->ball_r;
        g->velocity_y = abs(g->velocity_y);
    }

    next_y = g->ball_y + g->ball_r;
    if (g->velocity_y > 0 &&
        old_y + g->ball_r <= g->paddle_y &&
        next_y >= g->paddle_y &&
        g->ball_x + g->ball_r >= g->paddle_x &&
        g->ball_x - g->ball_r <= g->paddle_x + g->paddle_w) {
        int offset = g->ball_x - (g->paddle_x + g->paddle_w / 2);
        int max_x = g->base_speed + 4 + g->score / 50;
        int divisor;
        if (max_x > 13) max_x = 13;
        divisor = g->paddle_w / (max_x * 2);
        if (divisor < 1) divisor = 1;
        g->velocity_x = offset / divisor;
        if (g->velocity_x >= 0 && g->velocity_x < 2) g->velocity_x = 2;
        if (g->velocity_x < 0 && g->velocity_x > -2) g->velocity_x = -2;
        g->velocity_y = -abs(g->velocity_y);
        if (g->velocity_y > -12 && g->score > 0 &&
            g->score % 40 == 0)
            g->velocity_y--;
        g->ball_y = g->paddle_y - g->ball_r;
        g->score += 10;
        if (g->score > g->best) g->best = g->score;
        game_ui_draw_score(&g->ui, g->score, g->best);
    }

    erase_ball(g, old_x, old_y);
    if (old_paddle != g->paddle_x)
        erase_paddle(g, old_paddle);

    if (g->ball_y - g->ball_r > g->field_y + g->field_h) {
        finish_game(g);
        return;
    }
    draw_dynamic(g);
}

static int ball_game_loop(game_menu_ctx_t *ctx, int *best)
{
    ball_game_t g;
    long next_frame;
    int base_speed = ctx->difficulty == GAME_DIFF_EASY ? 4 :
                     ctx->difficulty == GAME_DIFF_HARD ? 7 : 5;
    int paddle_w = ctx->difficulty == GAME_DIFF_EASY ? 170 :
                   ctx->difficulty == GAME_DIFF_HARD ? 115 : 140;
    memset(&g, 0, sizeof(g));
    game_ui_layout(&g.ui);
    g.best = *best;
    reset_game(&g, base_speed, paddle_w);

    game_ui_draw_shell(&g.ui, "BOUNCE", g.score, g.best,
                       ctx->difficulty, "DRAG PADDLE OR USE KEYS");
    game_ui_draw_dpad(&g.ui);
    draw_field(&g);
    draw_dynamic(&g);
    next_frame = monotonic_ms() + FRAME_MS;

    for (;;) {
        input_event_t ev;
        int r = input_poll_raw(&ev);
        if (r == 0 && ev.type != INPUT_NONE) {
            game_ui_action_t action = game_ui_hit(&g.ui, ev.x, ev.y);
            if (ev.type == INPUT_QUIT) return -1;
            if (ev.type == INPUT_KEY_BACK) break;

            if (ev.type == INPUT_TOUCH_DOWN) {
                g.touching = 1;
                if (action == GAME_UI_LEFT) g.held_direction = -1;
                else if (action == GAME_UI_RIGHT) g.held_direction = 1;
            } else if (ev.type == INPUT_TOUCH_MOVE) {
                if (action == GAME_UI_LEFT) g.held_direction = -1;
                else if (action == GAME_UI_RIGHT) g.held_direction = 1;
                else if (ev.x >= g.field_x &&
                         ev.x < g.field_x + g.field_w &&
                         ev.y >= g.field_y &&
                         ev.y < g.field_y + g.field_h) {
                    int old_paddle_x = g.paddle_x;
                    g.held_direction = 0;
                    g.paddle_x = ev.x - g.paddle_w / 2;
                    clamp_paddle(&g);
                    if (!g.paused && !g.game_over &&
                        old_paddle_x != g.paddle_x) {
                        erase_paddle(&g, old_paddle_x);
                        draw_dynamic(&g);
                    }
                }
            } else if (ev.type == INPUT_TOUCH_UP ||
                       ev.type == INPUT_TAP) {
                g.touching = 0;
                g.held_direction = 0;
            }

            if (action == GAME_UI_BACK &&
                (ev.type == INPUT_TAP || ev.type == INPUT_TOUCH_UP))
                break;
            if (action == GAME_UI_PAUSE &&
                (ev.type == INPUT_TAP || ev.type == INPUT_TOUCH_UP) &&
                !g.game_over) {
                g.paused = !g.paused;
                if (g.paused)
                    game_ui_draw_overlay(&g.ui, "PAUSED",
                                         "Physics clock stopped", 0);
                else {
                    draw_field(&g);
                    draw_dynamic(&g);
                    next_frame = monotonic_ms() + FRAME_MS;
                }
                continue;
            }
            if (g.game_over &&
                (ev.type == INPUT_TOUCH_DOWN || ev.type == INPUT_TAP) &&
                ev.x >= g.ui.play_x && ev.x < g.ui.play_x + g.ui.play_w &&
                ev.y >= g.ui.play_y && ev.y < g.ui.play_y + g.ui.play_h) {
                game_menu_add_score(ctx, g.score);
                reset_game(&g, base_speed, paddle_w);
                game_ui_draw_score(&g.ui, g.score, g.best);
                draw_field(&g);
                draw_dynamic(&g);
                next_frame = monotonic_ms() + FRAME_MS;
            }
        }

        if (!g.paused && !g.game_over && monotonic_ms() >= next_frame) {
            update_physics(&g);
            next_frame += FRAME_MS;
            if (next_frame < monotonic_ms())
                next_frame = monotonic_ms() + FRAME_MS;
        }
        usleep(3000);
    }

    game_menu_add_score(ctx, g.score);
    *best = g.best;
    return 0;
}

int ball_run(void)
{
    game_menu_ctx_t ctx;
    int best = 0;
    game_menu_init(&ctx);
    ctx.title = "BOUNCE";
    ctx.title_color = 0xFFFFC857u;
    srand((unsigned)time(NULL));

    for (;;) {
        gmenu_result_t result = game_menu_run(&ctx);
        if (result == GMENU_EXIT) return 0;
        if (result == GMENU_START_GAME &&
            ball_game_loop(&ctx, &best) < 0)
            return -1;
    }
}
