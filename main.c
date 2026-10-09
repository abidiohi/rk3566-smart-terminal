/* ============================================================================
 * 文件: main.c
 * 说明: RK3566 GUI 系统主程序入口
 *      启动流程:
 *        1. 初始化 framebuffer (ui_init)
 *        2. 初始化触摸输入 (input_init)
 *        3. 播放开机动画 (boot_anim_play)
 *        4. 弹出登录界面 (默认账户 123 / 123)
 *        5. 进入主界面事件循环:
 *             - 点击按钮 → 跳转到对应子界面
 *             - 左右滑动 → 切换主界面页
 *             - 点击 Power → 播放关机动画并退出
 *        6. 资源清理
 * ==========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "ui.h"
#include "app_ui.h"
#include "input.h"
#include "main_ui.h"
#include "album.h"
#include "boot_anim.h"
#include "login.h"
#include "paint.h"
#include "ball.h"
#include "snake.h"
#include "gomoku.h"

/* 默认资源根目录: 优先用环境变量 RK_GUI_RES, 否则使用 ./res */
static const char *get_res_root(void)
{
    const char *e = getenv("RK_GUI_RES");
    if (e && e[0]) return e;
    return "./res";
}

/* 全局退出标志 */
static volatile sig_atomic_t g_should_exit = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_should_exit = 1;
}

/* 通用等待点击 (用于占位页) */
static void wait_tap_to_return(void)
{
    for (;;) {
        if (g_should_exit) return;
        input_event_t ev;
        int r = input_wait_event(&ev, 1000);
        if (r == 0) {
            if (ev.type == INPUT_QUIT) return;
            if (ev.type == INPUT_TAP) return;
            if (ev.type == INPUT_KEY_BACK) return;
        }
    }
}

static void show_about(void)
{
    app_rect_t panel = {
        g_screen_w / 2 - 330, g_screen_h / 2 - 190, 660, 380
    };
    char build[64];
    ui_clear(APP_COLOR_BG);
    ui_fill_rect(0, 0, g_screen_w, 72, APP_COLOR_HEADER);
    ui_fill_rect(0, 70, g_screen_w, 2, APP_COLOR_PRIMARY);
    ui_draw_text(24, 20, "ABOUT", UI_COLOR_WHITE, APP_COLOR_HEADER, 2);
    app_draw_surface(&panel, APP_COLOR_SURFACE, APP_COLOR_BORDER, 8);
    app_draw_icon(APP_ICON_ABOUT, panel.x + 78, panel.y + 82,
                  64, APP_COLOR_PRIMARY);
    ui_draw_text(panel.x + 138, panel.y + 48,
                 "RK3566 SMART SYSTEM", UI_COLOR_WHITE,
                 APP_COLOR_SURFACE, 3);
    ui_draw_text(panel.x + 138, panel.y + 105,
                 "Framebuffer touch interface", APP_COLOR_MUTED,
                 APP_COLOR_SURFACE, 1);
    snprintf(build, sizeof(build), "BUILD  %s  %s", __DATE__, __TIME__);
    ui_draw_text(panel.x + 42, panel.y + 180, build,
                 APP_COLOR_MUTED, APP_COLOR_SURFACE, 1);
    ui_draw_text(panel.x + 42, panel.y + 222,
                 "Album  Paint  Bounce  Snake  Gomoku",
                 UI_COLOR_WHITE, APP_COLOR_SURFACE, 1);
    ui_fill_round_rect(panel.x + 42, panel.y + 292,
                       panel.w - 84, 52, 6, APP_COLOR_SURFACE_2);
    ui_draw_text_center(panel.x + panel.w / 2, panel.y + 318,
                        "TAP OR PRESS BACK TO RETURN",
                        APP_COLOR_PRIMARY, APP_COLOR_SURFACE_2, 1);
    wait_tap_to_return();
}

static void show_password_updated(void)
{
    app_rect_t panel = {
        g_screen_w / 2 - 270, g_screen_h / 2 - 130, 540, 260
    };
    ui_clear(APP_COLOR_BG);
    app_draw_surface(&panel, APP_COLOR_SURFACE, APP_COLOR_PRIMARY, 8);
    app_draw_icon(APP_ICON_SECURITY, panel.x + 82, panel.y + 82,
                  58, APP_COLOR_PRIMARY);
    ui_draw_text(panel.x + 136, panel.y + 48,
                 "PASSWORD UPDATED", UI_COLOR_WHITE,
                 APP_COLOR_SURFACE, 2);
    ui_draw_text(panel.x + 136, panel.y + 92,
                 "The new password is active",
                 APP_COLOR_MUTED, APP_COLOR_SURFACE, 1);
    ui_fill_round_rect(panel.x + 36, panel.y + 176,
                       panel.w - 72, 48, 6, APP_COLOR_SURFACE_2);
    ui_draw_text_center(panel.x + panel.w / 2, panel.y + 200,
                        "TAP TO RETURN", APP_COLOR_PRIMARY,
                        APP_COLOR_SURFACE_2, 1);
    wait_tap_to_return();
}

/* 子界面分发 */
static void dispatch_button(int btn_id)
{
    const char *root = get_res_root();
    char path[512];

    if (g_should_exit) return;

    switch (btn_id) {
    case MAIN_BTN_ALBUM:
        snprintf(path, sizeof(path), "%s/album", root);
        printf("[main] Album: %s\n", path);
        album_run(path);
        break;

    case MAIN_BTN_PAINT:
        printf("[main] Paint\n");
        paint_run();
        break;

    case MAIN_BTN_SET_PASS:
        printf("[main] Set password\n");
        if (login_set_password() < 0) return;
        show_password_updated();
        break;

    case MAIN_BTN_BALL:
        printf("[main] Ball game\n");
        ball_run();
        break;

    case MAIN_BTN_SNAKE:
        printf("[main] Snake game\n");
        snake_run();
        break;

    case MAIN_BTN_GOMOKU:
        printf("[main] Gomoku game\n");
        gomoku_run();
        break;

    /* MAIN_BTN_MUSIC / VIDEO / SETTINGS 已从主界面移除, 不再处理 */
    /* MAIN_BTN_LOGIN 改名为 MAIN_BTN_SET_PASS, 见上面 */

    case MAIN_BTN_ABOUT:
        show_about();
        break;

    case MAIN_BTN_POWER:
        printf("[main] Shutdown\n");
        shutdown_anim_play(root, 50);
        g_should_exit = 1;
        break;

    case MAIN_BTN_NONE:
    default:
        break;
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    printf("\n=== RK3566 GUI 系统 v2.0 启动 ===\n");

    /* 1. framebuffer */
    if (ui_init() < 0) {
        fprintf(stderr, "[main] fb 初始化失败\n");
        return 1;
    }
    printf("[main] 屏幕: %dx%d\n", g_screen_w, g_screen_h);

    /* 2. 输入 */
    if (input_init(NULL, g_screen_w, g_screen_h) < 0) {
        fprintf(stderr, "[main] 输入设备初始化失败\n");
        ui_close();
        return 1;
    }

    /* 3. 开机动画 */
    boot_anim_play(get_res_root(), 50, 1);

    /* 4. 登录: 开机动画播放完毕后再触发
     *    可通过环境变量 RK_SKIP_LOGIN=1 跳过 (调试用)
     *    默认账户: 123 / 123
     */
    if (!getenv("RK_SKIP_LOGIN") ||
        getenv("RK_SKIP_LOGIN")[0] != '1') {
        printf("[main] 登录验证 (默认 123 / 123)\n");
        if (login_run() < 0) {
            fprintf(stderr, "[main] 登录失败, 退出\n");
            input_close();
            ui_close();
            return 1;
        }
    }

    /* 5. 主循环 */
    while (!g_should_exit) {
        int btn = main_ui_run();
        if (g_should_exit) break;
        if (btn == MAIN_BTN_NONE) continue;
        dispatch_button(btn);
    }

    /* 6. 清理 */
    printf("[main] 清理资源\n");
    input_close();
    ui_close();

    printf("[main] 退出完成\n");
    return 0;
}
