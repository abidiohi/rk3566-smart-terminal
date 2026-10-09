/* ============================================================================
 * 文件: login.c
 * 说明: 账号密码登录 + 设置密码 模块实现
 *      - 触摸键盘: 26字母 + 数字 + 删除 + 切换 (大/小写) + OK
 *      - 默认账户: 123 / 密码可改
 *      - 校验逻辑: strcmp 简单比对 (演示用途)
 *      - 失败计数: 3 次后允许 Skip 按钮 (登录调试用)
 *      - login_set_password() 供主界面"设置密码"入口调用, 改内存中的密码
 * ==========================================================================*/
#include "login.h"
#include "app_ui.h"
#include "ui.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 账户名固定, 密码可被 login_set_password() 运行时改写.
 * 真实系统应做哈希并加密存储; 这里只做演示. */
#define MAX_USER_LEN  16
#define MAX_PASS_LEN  12
static char VALID_USER[MAX_USER_LEN + 2] = "123";
static char VALID_PASS[MAX_PASS_LEN + 2] = "123";

#define MAX_FAIL      3

/* 屏幕键盘布局: 3 行字母 + 1 行数字 + 1 行功能 */
typedef struct {
    const char *keys[3][10];
    int         cols[3];
} kb_layout_t;

static const kb_layout_t KB_LOWER = {
    { { "q","w","e","r","t","y","u","i","o","p" },
      { "a","s","d","f","g","h","j","k","l",""   },
      { "z","x","c","v","b","n","m","","",""     } },
    { 10, 9, 7 }
};
static const kb_layout_t KB_UPPER = {
    { { "Q","W","E","R","T","Y","U","I","O","P" },
      { "A","S","D","F","G","H","J","K","L",""   },
      { "Z","X","C","V","B","N","M","","",""     } },
    { 10, 9, 7 }
};

static const char *NUM_KEYS[10] = {
    "0","1","2","3","4","5","6","7","8","9"
};

/* 绘制输入框 */
static void draw_input_box(int x, int y, int w, int h,
                            const char *label, const char *text,
                            int masked, int active)
{
    unsigned int bg = active ? APP_COLOR_SURFACE_2 : APP_COLOR_SURFACE;
    unsigned int border = active ? APP_COLOR_PRIMARY : APP_COLOR_BORDER;
    ui_fill_rect(x, y, w, h, bg);
    ui_draw_rect(x, y, w, h, border);

    /* 标签 */
    if (label) {
        ui_draw_text(x - 100, y + (h - 16) / 2, (char *)label,
                     APP_COLOR_MUTED, APP_COLOR_BG, 1);
    }
    /* 内容 (密码模式用 * 遮罩) */
    char buf[64];
    if (masked) {
        int n = (int)strlen(text);
        if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;
        for (int i = 0; i < n; i++) buf[i] = '*';
        buf[n] = '\0';
    } else {
        snprintf(buf, sizeof(buf), "%s", text);
    }
    ui_draw_text(x + 8, y + (h - 16) / 2, buf,
                 UI_COLOR_WHITE, bg, 2);
    /* 活跃光标 */
    if (active) {
        int tw = (int)strlen(buf) * 8 * 2;
        int cx = x + 8 + tw + 2;
        ui_draw_vline(cx, y + 4, h - 8, APP_COLOR_PRIMARY);
    }
}

/* 绘制键盘一行 (返回每个键的热区按钮数) */
static int draw_kb_row(int y0, int kh, int gap,
                        const char **keys, int n, int x0, int kw,
                        ui_button_t *btns)
{
    for (int i = 0; i < n; i++) {
        int x = x0 + i * (kw + gap);
        btns[i].x = x; btns[i].y = y0; btns[i].w = kw; btns[i].h = kh;
        btns[i].id = i;
        btns[i].label = (char *)keys[i];
        if (keys[i] && keys[i][0]) {
            ui_fill_round_rect(x, y0, kw, kh, 4, APP_COLOR_SURFACE);
            ui_draw_round_rect(x, y0, kw, kh, 4, APP_COLOR_BORDER);
            ui_draw_text_center(x + kw / 2, y0 + kh / 2,
                                (char *)keys[i],
                                UI_COLOR_WHITE, APP_COLOR_SURFACE, 2);
        }
    }
    return n;
}

/* 工具: 在指定 buf 追加/删除字符 (限 max_len) */
static void buf_append(char *buf, int max_len, const char *s)
{
    int n = (int)strlen(buf);
    int sl = (int)strlen(s);
    if (n + sl >= max_len) return;
    memcpy(buf + n, s, sl + 1);
}
static void buf_del(char *buf)
{
    int n = (int)strlen(buf);
    if (n > 0) buf[n - 1] = '\0';
}

/* --------------------------------------------------------------------------
 * 通用键盘输入弹窗 (单字段)
 *  - title  : 顶部标题文字
 *  - label  : 输入框左侧标签文字 (可空)
 *  - hint   : 输入框下方提示 (可空, 黄色)
 *  - err    : 错误提示 (可空, 红色); 优先于 hint
 *  - buf    : 输出缓冲 (进入时清空, 返回时含输入内容)
 *  - max_len: 缓冲容量
 *  - masked : 1=密码遮罩, 0=明文
 *  返回: 0 = OK 按下, -1 = QUIT 退出
 * -------------------------------------------------------------------------- */
static int keyboard_prompt(const char *title, const char *label,
                            const char *hint, const char *err,
                            char *buf, int max_len, int masked)
{
    buf[0] = '\0';
    int shift = 0;

    ui_clear(APP_COLOR_BG);
 
    /* 键盘区域
     * 布局 (从上到下):
     *   [Letter Row 0: qwertyuiop]
     *   [Letter Row 1: asdfghjkl_]
     *   [Letter Row 2: zxcvbnm___]
     *   [Number Row  0 1 2 3 4 5 6 7 8 9]   <- 始终显示, 数字密码友好
     *   [Shift] [Del] [OK] [Cancel]
     * 键高 kb_kh 随屏幕高度动态计算, 保证 4 行 + 间隙始终在屏内.
     */
    int kb_top = 240;
    int kb_x0 = 30;
    int kb_gap = 8;
    int kb_rows = 3;
    int kb_kh = (g_screen_h - 240 - 20 - 4 * kb_gap) / 4;
    if (kb_kh > 60) kb_kh = 60;
    if (kb_kh < 36) kb_kh = 36;
    int kb_kw = (g_screen_w - 60 - 9 * kb_gap) / 10;
    if (kb_kw < 32) kb_kw = 32;
    int num_y = kb_top + kb_rows * (kb_kh + kb_gap);
    int fn_y  = num_y + (kb_kh + kb_gap);

    /* 输入框坐标 */
    int box_x = g_screen_w / 2 - 200;
    int box_w = 400, box_h = 50;
    int in_y  = 130;

    for (;;) {
        /* 重绘 */
        ui_clear(APP_COLOR_BG);
        ui_fill_rect(0, 0, g_screen_w, 60, APP_COLOR_HEADER);
        ui_fill_rect(0, 58, g_screen_w, 2, APP_COLOR_PRIMARY);
        ui_draw_text_center(g_screen_w / 2, 20, (char *)title,
                            UI_COLOR_WHITE, 0xFF000000u, 3);

        if (err && err[0]) {
            ui_draw_text_center(g_screen_w / 2, 80, (char *)err,
                                APP_COLOR_DANGER, APP_COLOR_BG, 1);
        } else if (hint && hint[0]) {
            ui_draw_text_center(g_screen_w / 2, 80, (char *)hint,
                                APP_COLOR_AMBER, APP_COLOR_BG, 1);
        }

        draw_input_box(box_x, in_y, box_w, box_h, label,
                       buf, masked, 1);

        /* 绘制键盘 */
        const kb_layout_t *kb = shift ? &KB_UPPER : &KB_LOWER;
        ui_button_t kb_btns[10];

        for (int r = 0; r < kb_rows; r++) {
            int n = (*kb).cols[r];
            int y0 = kb_top + r * (kb_kh + kb_gap);
            draw_kb_row(y0, kb_kh, kb_gap,
                        (*kb).keys[r], n, kb_x0, kb_kw, kb_btns);
        }
        /* 数字栏: 始终显示, 用深色背景突出 */
        {
            ui_fill_rect(kb_x0 - 2, num_y - 2,
                         (kb_kw + kb_gap) * 10 - kb_gap + 4,
                         kb_kh + 4, APP_COLOR_PANEL);
            draw_kb_row(num_y, kb_kh, kb_gap,
                        NUM_KEYS, 10, kb_x0, kb_kw, kb_btns);
        }

        /* 第 5 行: Shift / Del / OK / Cancel */
        int rb_y = fn_y;
        int bw4 = (g_screen_w - 60 - 3 * 12) / 4;
        const char *l4[4] = {
            shift ? "abc" : "ABC",
            "Del",
            "OK",
            "Cancel"
        };
        for (int i = 0; i < 4; i++) {
            int x = 30 + i * (bw4 + 12);
            unsigned int fill = (i == 2) ? APP_COLOR_PRIMARY
                              : (i == 3) ? APP_COLOR_DANGER
                                         : APP_COLOR_SURFACE_2;
            ui_fill_round_rect(x, rb_y, bw4, kb_kh, 5, fill);
            ui_draw_round_rect(x, rb_y, bw4, kb_kh, 5, APP_COLOR_BORDER);
            ui_draw_text_center(x + bw4 / 2, rb_y + kb_kh / 2,
                                (char *)l4[i],
                                UI_COLOR_WHITE, 0xFF000000u, 2);
        }

        /* 事件 */
        input_event_t ev;
        int r = input_wait_event(&ev, 60000);
        if (r == 0 && ev.type == INPUT_QUIT) return -1;
        if (r < 0) continue;
        if (ev.type != INPUT_TAP) continue;

        int px = ev.x, py = ev.y;

        /* 数字栏 */
        if (py >= num_y && py < num_y + kb_kh) {
            int idx = (px - kb_x0) / (kb_kw + kb_gap);
            if (idx >= 0 && idx < 10) {
                buf_append(buf, max_len, NUM_KEYS[idx]);
                continue;
            }
        }

        /* 字母键盘 (3 行) */
        if (py >= kb_top && py < kb_top + kb_rows * (kb_kh + kb_gap)) {
            int row = (py - kb_top) / (kb_kh + kb_gap);
            if (row >= 0 && row < kb_rows) {
                int n = (*kb).cols[row];
                int idx = (px - kb_x0) / (kb_kw + kb_gap);
                if (idx >= 0 && idx < n) {
                    const char *k = (*kb).keys[row][idx];
                    if (k && k[0]) {
                        buf_append(buf, max_len, k);
                        continue;
                    }
                }
            }
        }

        /* 第 5 行: Shift / Del / OK / Cancel */
        if (py >= rb_y && py < rb_y + kb_kh) {
            int idx = (px - 30) / (bw4 + 12);
            if (idx == 0) {                 /* Shift */
                shift = !shift;
                continue;
            } else if (idx == 1) {          /* Del */
                buf_del(buf);
                continue;
            } else if (idx == 2) {          /* OK */
                if ((int)strlen(buf) == 0) continue;
                return 0;
            } else if (idx == 3) {          /* Cancel */
                return -1;
            }
        }
    }
}

/* --------------------------------------------------------------------------
 * 对外 API
 * -------------------------------------------------------------------------- */

/* 启动时登录: 输入账号 + 密码, 校验通过返回 0
 *  失败 3 次后允许 Cancel (跳过) 也返回 0
 */
int login_run(void)
{
    char user[MAX_USER_LEN + 2];
    char pass[MAX_PASS_LEN + 2];
    int  fail = 0;
    char err[64] = "";

    for (;;) {
        if (keyboard_prompt("User Login", "User:", NULL, err,
                            user, sizeof(user), 0) < 0) {
            return -1;
        }
        if (keyboard_prompt("User Login", "Pass:", NULL, err,
                            pass, sizeof(pass), 1) < 0) {
            return -1;
        }
        if (strcmp(user, VALID_USER) == 0 &&
            strcmp(pass, VALID_PASS) == 0) {
            return 0;
        }
        fail++;
        if (fail >= MAX_FAIL) {
            return 0;   /* 调试: 多次失败直接放行 */
        }
        snprintf(err, sizeof(err),
                 "Wrong! %d attempt(s) left (try 123 / 123)",
                 MAX_FAIL - fail);
    }
}

/* 主界面"设置密码"入口:
 *  1) 验证当前密码
 *  2) 输入新密码
 *  3) 再次输入新密码
 *  4) 两次一致则更新内存中的密码
 *  返回: 0 = 修改成功, -1 = 用户取消
 */
int login_set_password(void)
{
    char cur[MAX_PASS_LEN + 2];
    char n1 [MAX_PASS_LEN + 2];
    char n2 [MAX_PASS_LEN + 2];
    char err[64] = "";

    /* 1. 验证当前密码 (最多 3 次) */
    int fail = 0;
    for (;;) {
        if (keyboard_prompt("Verify Current Password",
                            "Old:", "Default: 123", err,
                            cur, sizeof(cur), 1) < 0) {
            return -1;
        }
        if (strcmp(cur, VALID_PASS) == 0) break;
        fail++;
        if (fail >= MAX_FAIL) {
            snprintf(err, sizeof(err), "Too many fails, aborted");
            /* 给个错误提示, 仍允许用户取消 */
            keyboard_prompt("Failed", NULL, err, NULL,
                            cur, sizeof(cur), 1);
            return -1;
        }
        snprintf(err, sizeof(err), "Wrong! %d attempt(s) left",
                 MAX_FAIL - fail);
    }

    /* 2. 输入新密码 (要求非空) */
    for (;;) {
        if (keyboard_prompt("Set New Password",
                            "New:", "1..12 chars", err,
                            n1, sizeof(n1), 1) < 0) {
            return -1;
        }
        if (strlen(n1) == 0) {
            snprintf(err, sizeof(err), "Password cannot be empty");
            continue;
        }
        break;
    }

    /* 3. 确认新密码 */
    err[0] = '\0';
    for (;;) {
        if (keyboard_prompt("Confirm New Password",
                            "Again:", "Enter again", err,
                            n2, sizeof(n2), 1) < 0) {
            return -1;
        }
        if (strcmp(n1, n2) == 0) break;
        snprintf(err, sizeof(err), "Mismatch, try again");
    }

    /* 4. 保存新密码 */
    strncpy(VALID_PASS, n1, sizeof(VALID_PASS) - 1);
    VALID_PASS[sizeof(VALID_PASS) - 1] = '\0';
    return 0;
}
