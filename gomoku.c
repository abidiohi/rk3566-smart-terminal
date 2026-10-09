/* ============================================================================
 * 文件: gomoku.c
 * 说明: 五子棋实现
 *      - 棋盘 15x15
 *      - 点击棋盘: 找到最近交叉点 (CELL=屏幕宽/15)
 *      - 5 连判定: 4 个方向 (水平/垂直/主对角/反对角)
 *      - 黑先白后, 黑=0, 白=1
 * ==========================================================================*/
#include "gomoku.h"
#include "app_ui.h"
#include "ui.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define N         15
#define MAX_STEP  (N * N)

static int g_board[N][N];
static int g_cur;     /* 0=黑, 1=白 */
static int g_step_count;
static int g_winner;  /* -1 无, 0 黑赢, 1 白赢, 2 平局 */
static int g_last_x, g_last_y;
static int g_history_x[MAX_STEP], g_history_y[MAX_STEP];

/* 棋盘像素布局 */
static int g_cell;
static int g_x0, g_y0;  /* 左上交点像素 */
static int g_x1, g_y1;  /* 右下交点像素 */

static void recalc_geometry(void)
{
    int top = 74, bottom = 90;
    int avail = g_screen_h - top - bottom;
    g_cell = avail / (N - 1);
    if (g_cell < 12) g_cell = 12;
    int board_w = g_cell * (N - 1);
    int board_h = g_cell * (N - 1);
    g_x0 = (g_screen_w - board_w) / 2;
    g_y0 = top + (avail - board_h) / 2;
    g_x1 = g_x0 + board_w;
    g_y1 = g_y0 + board_h;
}

static void reset_game(void)
{
    memset(g_board, -1, sizeof(g_board));
    g_cur = 0;
    g_step_count = 0;
    g_winner = -1;
    g_last_x = g_last_y = -1;
    recalc_geometry();
}

static int check_win(int x, int y, int who)
{
    int dirs[4][2] = { {1,0}, {0,1}, {1,1}, {1,-1} };
    for (int d = 0; d < 4; d++) {
        int dx = dirs[d][0], dy = dirs[d][1];
        int cnt = 1;
        for (int s = 1; s < 5; s++) {
            int nx = x + dx * s, ny = y + dy * s;
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) break;
            if (g_board[ny][nx] != who) break;
            cnt++;
        }
        for (int s = 1; s < 5; s++) {
            int nx = x - dx * s, ny = y - dy * s;
            if (nx < 0 || ny < 0 || nx >= N || ny >= N) break;
            if (g_board[ny][nx] != who) break;
            cnt++;
        }
        if (cnt >= 5) return 1;
    }
    return 0;
}

static void draw_full(void)
{
    const unsigned int board_bg = 0xFFD6B06Cu;
    int board_pad = 16;
    int by = g_screen_h - 78;
    ui_clear(APP_COLOR_BG);
    ui_fill_round_rect(g_x0 - board_pad, g_y0 - board_pad,
                       g_x1 - g_x0 + board_pad * 2,
                       g_y1 - g_y0 + board_pad * 2,
                       7, board_bg);
    ui_draw_round_rect(g_x0 - board_pad, g_y0 - board_pad,
                       g_x1 - g_x0 + board_pad * 2,
                       g_y1 - g_y0 + board_pad * 2,
                       7, APP_COLOR_AMBER);

    char buf[64];
    if (g_winner == 2)
        snprintf(buf, sizeof(buf), "DRAW  -  START A NEW ROUND");
    else if (g_winner >= 0)
        snprintf(buf, sizeof(buf), "%s WINS  -  START A NEW ROUND",
                 g_winner == 0 ? "BLACK" : "WHITE");
    else
        snprintf(buf, sizeof(buf), "STEP %03d  |  %s TO MOVE",
                 g_step_count, g_cur == 0 ? "BLACK" : "WHITE");
    ui_fill_rect(0, 0, g_screen_w, 64, APP_COLOR_HEADER);
    ui_fill_rect(0, 62, g_screen_w, 2, APP_COLOR_AMBER);
    ui_draw_text(22, 16, "GOMOKU", UI_COLOR_WHITE, APP_COLOR_HEADER, 3);
    {
        int tw = (int)strlen(buf) * 8;
        ui_draw_text(g_screen_w - tw - 22, 24, buf,
                     g_winner >= 0 ? APP_COLOR_AMBER : APP_COLOR_MUTED,
                     APP_COLOR_HEADER, 1);
    }

    /* 网格 */
    for (int i = 0; i < N; i++) {
        ui_draw_hline(g_x0, g_y0 + i * g_cell, g_x1 - g_x0, UI_COLOR_BLACK);
        ui_draw_vline(g_x0 + i * g_cell, g_y0, g_y1 - g_y0, UI_COLOR_BLACK);
    }
    /* 中心点 */
    int cx = g_x0 + (N / 2) * g_cell;
    int cy = g_y0 + (N / 2) * g_cell;
    ui_fill_circle(cx, cy, 4, UI_COLOR_BLACK);

    /* 棋子 */
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            if (g_board[y][x] < 0) continue;
            unsigned int c = (g_board[y][x] == 0) ? UI_COLOR_BLACK : UI_COLOR_WHITE;
            int px = g_x0 + x * g_cell;
            int py = g_y0 + y * g_cell;
            int r = g_cell / 2 - 2;
            ui_fill_circle(px, py, r, c);
            ui_draw_circle(px, py, r, 0xFF6A4B28u);
        }
    }
    /* 最后落子标记 (小红点) */
    if (g_last_x >= 0) {
        int px = g_x0 + g_last_x * g_cell;
        int py = g_y0 + g_last_y * g_cell;
        ui_fill_circle(px, py, 4, APP_COLOR_DANGER);
    }

    ui_fill_rect(0, by, g_screen_w, 78, APP_COLOR_HEADER);
    {
        int bw = 156, bh = 48, gap = 16;
        int x0 = (g_screen_w - 3 * bw - 2 * gap) / 2;
        ui_button_t btns[3];
        const char *lbl[3] = { "UNDO", "NEW ROUND", "BACK" };
        int ids[3] = { 300, 301, 302 };
        for (int i = 0; i < 3; i++) {
            int x = x0 + i * (bw + gap);
            unsigned int fill = i == 2 ? APP_COLOR_DANGER :
                                i == 1 ? APP_COLOR_PRIMARY :
                                         APP_COLOR_SURFACE;
            unsigned int text = i == 1 ? APP_COLOR_BG : UI_COLOR_WHITE;
            btns[i].x = x; btns[i].y = by + 15;
            btns[i].w = bw; btns[i].h = bh;
            btns[i].id = ids[i];
            btns[i].label = lbl[i];
            ui_draw_button(&btns[i], fill, APP_COLOR_BORDER,
                           text, fill, i == 1 ? 1 : 2);
        }
    }
}

static int hit_button(int x, int y, int *id_out)
{
    int by = g_screen_h - 78;
    if (y < by + 15 || y >= by + 65) return 0;
    int bw = 156, gap = 16;
    int x0 = (g_screen_w - 3 * bw - 2 * gap) / 2;
    for (int i = 0; i < 3; i++) {
        int bx = x0 + i * (bw + gap);
        if (x >= bx && x < bx + bw) {
            *id_out = 300 + i;
            return 1;
        }
    }
    return 0;
}

/* 把点击坐标转换到棋盘交叉点, 返回是否有效 */
static int screen_to_board(int sx, int sy, int *bx_out, int *by_out)
{
    int bx = (sx - g_x0 + g_cell / 2) / g_cell;
    int by = (sy - g_y0 + g_cell / 2) / g_cell;
    if (bx < 0 || by < 0 || bx >= N || by >= N) return 0;
    /* 距离交叉点是否够近? 允许半格误差 */
    int px = g_x0 + bx * g_cell;
    int py = g_y0 + by * g_cell;
    int dx = sx - px, dy = sy - py;
    if (dx * dx + dy * dy > (g_cell * g_cell) / 2) return 0;
    *bx_out = bx;
    *by_out = by;
    return 1;
}

int gomoku_run(void)
{
    reset_game();
    draw_full();

    for (;;) {
        input_event_t ev;
        int r = input_wait_event(&ev, 60000);
        if (r == 0 && ev.type == INPUT_QUIT) return -1;
        if (r < 0) continue;

        if (ev.type == INPUT_KEY_BACK) return 0;
        if (ev.type != INPUT_TAP) continue;

        /* 按钮? */
        int id;
        if (hit_button(ev.x, ev.y, &id)) {
            if (id == 302) return 0;       /* Back */
            if (id == 301) { reset_game(); draw_full(); continue; }
            if (id == 300) {               /* Undo */
                if (g_step_count > 0) {
                    int undo_x = g_history_x[g_step_count - 1];
                    int undo_y = g_history_y[g_step_count - 1];
                    g_board[undo_y][undo_x] = -1;
                    g_step_count--;
                    g_cur = 1 - g_cur;
                    g_winner = -1;
                    if (g_step_count > 0) {
                        g_last_x = g_history_x[g_step_count - 1];
                        g_last_y = g_history_y[g_step_count - 1];
                    } else {
                        g_last_x = g_last_y = -1;
                    }
                    draw_full();
                }
                continue;
            }
        }

        if (g_winner >= 0) {
            reset_game();
            draw_full();
            continue;
        }

        /* 棋盘? */
        int bx, by;
        if (screen_to_board(ev.x, ev.y, &bx, &by)) {
            if (g_board[by][bx] >= 0) continue;
            g_board[by][bx] = g_cur;
            g_history_x[g_step_count] = bx;
            g_history_y[g_step_count] = by;
            g_last_x = bx; g_last_y = by;
            g_step_count++;
            if (check_win(bx, by, g_cur)) {
                g_winner = g_cur;
            } else if (g_step_count >= N * N) {
                g_winner = 2;
            } else {
                g_cur = 1 - g_cur;
            }
            draw_full();
        }
    }
}
