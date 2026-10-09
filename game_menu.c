#include "game_menu.h"
#include "app_ui.h"
#include "input.h"
#include "ui.h"

#include <stdio.h>
#include <string.h>

#define C_BG       APP_COLOR_BG
#define C_HEADER   APP_COLOR_HEADER
#define C_CARD     APP_COLOR_SURFACE
#define C_CARD_ALT APP_COLOR_SURFACE_2
#define C_BORDER   APP_COLOR_BORDER
#define C_ACCENT   APP_COLOR_PRIMARY
#define C_MUTED    APP_COLOR_MUTED
#define C_DANGER   APP_COLOR_DANGER

#define TOP_H       76
#define FOOT_H      44
#define MAX_ITEMS    5

typedef enum {
    PAGE_MAIN = 0,
    PAGE_DIFFICULTY,
    PAGE_LEADERBOARD,
    PAGE_SETTINGS
} menu_page_t;

typedef enum {
    ITEM_START = 0,
    ITEM_DIFFICULTY,
    ITEM_LEADERBOARD,
    ITEM_SETTINGS,
    ITEM_EXIT
} main_item_t;

typedef struct {
    int x, y, w, h;
} rect_t;

static int g_selected;

static int point_in(const rect_t *r, int x, int y)
{
    return x >= r->x && x < r->x + r->w &&
           y >= r->y && y < r->y + r->h;
}

static const char *difficulty_name(game_difficulty_t difficulty)
{
    static const char *names[] = { "EASY", "NORMAL", "HARD" };
    return names[(int)difficulty];
}

static int main_item_count(const game_menu_ctx_t *ctx)
{
    return ctx->settings_count > 0 ? 5 : 4;
}

static main_item_t main_item_at(const game_menu_ctx_t *ctx, int index)
{
    if (ctx->settings_count > 0 || index < 3)
        return (main_item_t)index;
    return ITEM_EXIT;
}

static const char *main_item_label(main_item_t item)
{
    static const char *labels[] = {
        "START GAME", "DIFFICULTY", "LEADERBOARD", "SETTINGS", "EXIT"
    };
    return labels[(int)item];
}

static void main_item_rect(const game_menu_ctx_t *ctx, int index, rect_t *r)
{
    int margin = 24;
    int gap = 16;
    int top = TOP_H + 24;
    int bottom = g_screen_h - FOOT_H - 20;
    int total_h = bottom - top;
    int first_w = (g_screen_w - margin * 2 - gap * 2) * 44 / 100;
    int small_w = (g_screen_w - margin * 2 - gap * 2 - first_w) / 2;
    int small_h = (total_h - gap) / 2;
    int x1 = margin + first_w + gap;
    int x2 = x1 + small_w + gap;

    if (index == 0) {
        r->x = margin; r->y = top; r->w = first_w; r->h = total_h;
    } else {
        int slot = index - 1;
        r->x = (slot & 1) ? x2 : x1;
        r->y = top + (slot / 2) * (small_h + gap);
        r->w = small_w; r->h = small_h;
    }
    (void)ctx;
}

static void list_item_rect(int index, int count, rect_t *r)
{
    int margin = 42;
    int gap = 18;
    int content_y = TOP_H + 46;
    int available = g_screen_h - content_y - FOOT_H - 34;
    int w = (g_screen_w - margin * 2 - gap * (count - 1)) / count;
    r->x = margin + index * (w + gap);
    r->y = content_y;
    r->w = w;
    r->h = available;
}

static void draw_header(const game_menu_ctx_t *ctx, const char *page)
{
    char title[96];
    ui_fill_rect(0, 0, g_screen_w, TOP_H, C_HEADER);
    ui_fill_rect(0, TOP_H - 2, g_screen_w, 2, ctx->title_color);
    snprintf(title, sizeof(title), "%s  /  %s", ctx->title, page);
    ui_draw_text(24, 20, title, UI_COLOR_WHITE, C_HEADER, 3);
}

static void draw_footer(const char *hint)
{
    int y = g_screen_h - FOOT_H;
    ui_fill_rect(0, y, g_screen_w, FOOT_H, C_HEADER);
    ui_draw_text_center(g_screen_w / 2, y + 14, hint,
                        C_MUTED, C_HEADER, 1);
}

static void draw_card(const rect_t *r, const char *label,
                      const char *detail, int selected,
                      unsigned int accent, int large)
{
    unsigned int fill = selected ? C_CARD_ALT : C_CARD;
    ui_fill_rect(r->x, r->y, r->w, r->h, fill);
    ui_fill_rect(r->x, r->y, selected ? 8 : 4, r->h, accent);
    ui_draw_rect(r->x, r->y, r->w, r->h,
                 selected ? accent : C_BORDER);
    ui_draw_text(r->x + 24, r->y + (large ? 38 : 30), label,
                 UI_COLOR_WHITE, fill, large ? 3 : 2);
    if (detail)
        ui_draw_text(r->x + 24, r->y + (large ? 92 : 72), detail,
                     selected ? accent : C_MUTED, fill, large ? 2 : 1);
    if (selected)
        ui_fill_circle(r->x + r->w - 24, r->y + 24, 7, accent);
}

static void draw_main(const game_menu_ctx_t *ctx)
{
    int i;
    int count = main_item_count(ctx);
    ui_clear(C_BG);
    draw_header(ctx, "GAME MENU");

    for (i = 0; i < count; i++) {
        rect_t r;
        main_item_t item = main_item_at(ctx, i);
        const char *detail = NULL;
        unsigned int accent = C_ACCENT;
        char detail_buffer[32];
        main_item_rect(ctx, i, &r);
        if (item == ITEM_START) detail = "READY FOR A NEW RUN";
        else if (item == ITEM_DIFFICULTY) {
            snprintf(detail_buffer, sizeof(detail_buffer), "CURRENT: %s",
                     difficulty_name(ctx->difficulty));
            detail = detail_buffer;
            accent = 0xFFFFC857u;
        } else if (item == ITEM_LEADERBOARD) {
            snprintf(detail_buffer, sizeof(detail_buffer), "%d SAVED SCORES",
                     ctx->lb_count);
            detail = detail_buffer;
            accent = 0xFF52A8FFu;
        } else if (item == ITEM_SETTINGS) {
            detail = "GAMEPLAY OPTIONS";
            accent = 0xFFB879FFu;
        } else {
            detail = "RETURN TO GAME HUB";
            accent = C_DANGER;
        }
        draw_card(&r, main_item_label(item), detail,
                  g_selected == i, accent, item == ITEM_START);
    }
    draw_footer("TAP A CARD  |  SWIPE UP/DOWN TO SELECT");
}

static void draw_difficulty(const game_menu_ctx_t *ctx)
{
    static const char *names[] = { "EASY", "NORMAL", "HARD" };
    static const char *details[] = {
        "RELAXED SPEED", "BALANCED PLAY", "FAST REACTIONS"
    };
    static const unsigned int accents[] = {
        0xFF3DDC97u, 0xFFFFC857u, 0xFFE85D75u
    };
    int i;
    ui_clear(C_BG);
    draw_header(ctx, "DIFFICULTY");
    for (i = 0; i < 3; i++) {
        rect_t r;
        list_item_rect(i, 3, &r);
        draw_card(&r, names[i], details[i], g_selected == i,
                  accents[i], 1);
        if ((int)ctx->difficulty == i)
            ui_draw_text(r.x + 24, r.y + r.h - 48, "ACTIVE",
                         accents[i], g_selected == i ? C_CARD_ALT : C_CARD, 2);
    }
    draw_footer("TAP TO APPLY  |  SWIPE LEFT OR BACK TO RETURN");
}

static void draw_leaderboard(const game_menu_ctx_t *ctx)
{
    int i;
    int x = g_screen_w / 2 - 360;
    int y = TOP_H + 28;
    int w = 720;
    int row_h = 40;
    ui_clear(C_BG);
    draw_header(ctx, "LEADERBOARD");
    ui_fill_rect(x, y, w, row_h, C_CARD_ALT);
    ui_draw_text(x + 24, y + 12, "RANK", C_MUTED, C_CARD_ALT, 1);
    ui_draw_text(x + w - 150, y + 12, "SCORE", C_MUTED, C_CARD_ALT, 1);
    if (ctx->lb_count == 0) {
        ui_draw_text_center(g_screen_w / 2, g_screen_h / 2 - 10,
                            "NO SCORES YET", C_MUTED, C_BG, 3);
        ui_draw_text_center(g_screen_w / 2, g_screen_h / 2 + 36,
                            "Complete a run to create the first record",
                            C_MUTED, C_BG, 1);
    } else {
        int show = ctx->lb_count > 10 ? 10 : ctx->lb_count;
        for (i = 0; i < show; i++) {
            char rank[12], score[24];
            unsigned int row = (i & 1) ? C_CARD_ALT : C_CARD;
            unsigned int accent = i == 0 ? 0xFFFFC857u : UI_COLOR_WHITE;
            snprintf(rank, sizeof(rank), "%02d", i + 1);
            snprintf(score, sizeof(score), "%05d", ctx->leaderboard[i].score);
            ui_fill_rect(x, y + (i + 1) * row_h, w, row_h - 2, row);
            ui_draw_text(x + 24, y + (i + 1) * row_h + 10,
                         rank, accent, row, 2);
            ui_draw_text(x + w - 150, y + (i + 1) * row_h + 10,
                         score, accent, row, 2);
        }
    }
    draw_footer("TAP, SWIPE LEFT OR BACK TO RETURN");
}

static void setting_row_rect(int index, rect_t *r)
{
    r->x = 100;
    r->y = TOP_H + 42 + index * 86;
    r->w = g_screen_w - 200;
    r->h = 66;
}

static void draw_settings(const game_menu_ctx_t *ctx)
{
    int i;
    ui_clear(C_BG);
    draw_header(ctx, "SETTINGS");
    for (i = 0; i < ctx->settings_count; i++) {
        rect_t r;
        char value[20];
        unsigned int fill;
        setting_row_rect(i, &r);
        fill = g_selected == i ? C_CARD_ALT : C_CARD;
        ui_fill_rect(r.x, r.y, r.w, r.h, fill);
        ui_draw_rect(r.x, r.y, r.w, r.h,
                     g_selected == i ? C_ACCENT : C_BORDER);
        ui_draw_text(r.x + 22, r.y + 22, ctx->settings[i].name,
                     UI_COLOR_WHITE, fill, 2);
        ui_fill_rect(r.x + r.w - 220, r.y + 9, 48, 48, C_HEADER);
        ui_fill_rect(r.x + r.w - 60, r.y + 9, 48, 48, C_HEADER);
        ui_draw_text_center(r.x + r.w - 196, r.y + 24, "-",
                            UI_COLOR_WHITE, C_HEADER, 2);
        ui_draw_text_center(r.x + r.w - 36, r.y + 24, "+",
                            UI_COLOR_WHITE, C_HEADER, 2);
        snprintf(value, sizeof(value), "%d", ctx->settings[i].value);
        ui_draw_text_center(r.x + r.w - 116, r.y + 24, value,
                            C_ACCENT, fill, 2);
    }
    draw_footer("TAP - / + TO ADJUST  |  SWIPE LEFT OR BACK TO RETURN");
}

static void draw_page(menu_page_t page, const game_menu_ctx_t *ctx)
{
    if (page == PAGE_MAIN) draw_main(ctx);
    else if (page == PAGE_DIFFICULTY) draw_difficulty(ctx);
    else if (page == PAGE_LEADERBOARD) draw_leaderboard(ctx);
    else draw_settings(ctx);
}

static int page_item_count(menu_page_t page, const game_menu_ctx_t *ctx)
{
    if (page == PAGE_MAIN) return main_item_count(ctx);
    if (page == PAGE_DIFFICULTY) return 3;
    if (page == PAGE_SETTINGS) return ctx->settings_count;
    return 0;
}

static int hit_page_item(menu_page_t page, const game_menu_ctx_t *ctx,
                         int x, int y)
{
    int i, count = page_item_count(page, ctx);
    for (i = 0; i < count; i++) {
        rect_t r;
        if (page == PAGE_MAIN) main_item_rect(ctx, i, &r);
        else if (page == PAGE_DIFFICULTY) list_item_rect(i, 3, &r);
        else setting_row_rect(i, &r);
        if (point_in(&r, x, y)) return i;
    }
    return -1;
}

static void adjust_setting(game_menu_ctx_t *ctx, int index, int x, int y)
{
    rect_t r;
    game_setting_t *setting;
    setting_row_rect(index, &r);
    setting = &ctx->settings[index];
    if (x >= r.x + r.w - 220 && x < r.x + r.w - 172 &&
        y >= r.y + 9 && y < r.y + 57) {
        setting->value -= setting->step;
        if (setting->value < setting->min_val)
            setting->value = setting->min_val;
    } else if (x >= r.x + r.w - 60 && x < r.x + r.w - 12 &&
               y >= r.y + 9 && y < r.y + 57) {
        setting->value += setting->step;
        if (setting->value > setting->max_val)
            setting->value = setting->max_val;
    }
}

gmenu_result_t game_menu_run(game_menu_ctx_t *ctx)
{
    menu_page_t page = PAGE_MAIN;
    g_selected = 0;
    draw_page(page, ctx);

    for (;;) {
        input_event_t ev;
        int count;
        if (input_wait_event(&ev, 500) < 0) continue;
        if (ev.type == INPUT_QUIT) return GMENU_EXIT;
        if (ev.type == INPUT_KEY_BACK) {
            if (page == PAGE_MAIN) return GMENU_EXIT;
            page = PAGE_MAIN;
            g_selected = 0;
            draw_page(page, ctx);
            continue;
        }
        if (page == PAGE_LEADERBOARD) {
            if (ev.type == INPUT_TAP || ev.type == INPUT_SWIPE_LEFT) {
                page = PAGE_MAIN;
                g_selected = 2;
                draw_page(page, ctx);
            }
            continue;
        }
        if (ev.type == INPUT_SWIPE_LEFT && page != PAGE_MAIN) {
            page = PAGE_MAIN;
            g_selected = 0;
            draw_page(page, ctx);
            continue;
        }

        count = page_item_count(page, ctx);
        if (ev.type == INPUT_SWIPE_UP && g_selected > 0) {
            g_selected--;
            draw_page(page, ctx);
            continue;
        }
        if (ev.type == INPUT_SWIPE_DOWN && g_selected + 1 < count) {
            g_selected++;
            draw_page(page, ctx);
            continue;
        }
        if (ev.type == INPUT_TAP) {
            int hit = hit_page_item(page, ctx, ev.x, ev.y);
            if (hit < 0) continue;
            g_selected = hit;
            if (page == PAGE_MAIN) {
                main_item_t item = main_item_at(ctx, hit);
                if (item == ITEM_START) return GMENU_START_GAME;
                if (item == ITEM_EXIT) return GMENU_EXIT;
                if (item == ITEM_DIFFICULTY) {
                    page = PAGE_DIFFICULTY;
                    g_selected = (int)ctx->difficulty;
                } else if (item == ITEM_LEADERBOARD) {
                    page = PAGE_LEADERBOARD;
                    g_selected = 0;
                } else {
                    page = PAGE_SETTINGS;
                    g_selected = 0;
                }
            } else if (page == PAGE_DIFFICULTY) {
                ctx->difficulty = (game_difficulty_t)hit;
                page = PAGE_MAIN;
                g_selected = 1;
            } else {
                adjust_setting(ctx, hit, ev.x, ev.y);
            }
            draw_page(page, ctx);
        }
    }
}

void game_menu_add_score(game_menu_ctx_t *ctx, int score)
{
    int i, pos;
    if (score <= 0) return;
    pos = ctx->lb_count;
    for (i = 0; i < ctx->lb_count; i++) {
        if (score > ctx->leaderboard[i].score) {
            pos = i;
            break;
        }
    }
    if (pos >= GAME_LB_MAX) return;
    i = ctx->lb_count < GAME_LB_MAX ? ctx->lb_count : GAME_LB_MAX - 1;
    for (; i > pos; i--)
        ctx->leaderboard[i] = ctx->leaderboard[i - 1];
    ctx->leaderboard[pos].score = score;
    if (ctx->lb_count < GAME_LB_MAX) ctx->lb_count++;
}

void game_menu_init(game_menu_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->difficulty = GAME_DIFF_MEDIUM;
}
