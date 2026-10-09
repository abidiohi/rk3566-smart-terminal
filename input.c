/* ============================================================================
 * 文件: input.c
 * 说明: 触摸/键盘输入事件抽象层实现
 *      解析 /dev/input/eventX 的 EV_ABS/EV_KEY/EV_SYN 事件流,
 *      识别按下-抬起轨迹, 计算出点击或滑动手势.
 *      自动将触摸原始坐标 (触摸 IC 报告范围) 映射到屏幕像素范围.
 * ==========================================================================*/
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <linux/input.h>

/* 默认设备节点 */
#define DEFAULT_INPUT_DEV   "/dev/input/event6"

/* 滑动阈值: 屏幕短轴约 1/10, 兼顾手指抖动与触发距离。 */
#define SWIPE_THRESHOLD_DIVISOR  10

/* 全局状态 */
static int g_fd = -1;
static int g_touch_min_x = 0, g_touch_max_x = 0;
static int g_touch_min_y = 0, g_touch_max_y = 0;
static int g_screen_w    = 0, g_screen_h    = 0;
static int g_coord_mapped = 0;     /* 是否需要做坐标映射 */
static volatile sig_atomic_t g_quit = 0;

/* 信号处理: 标记退出 */
static void input_signal_handler(int sig)
{
    (void)sig;
    g_quit = 1;
}

/* 工具: 取绝对值 */
static int abs_int(int v) { return v < 0 ? -v : v; }

/* 工具: 将触摸原始坐标映射到屏幕坐标 */
static int map_x(int v) {
    if (!g_coord_mapped) return v;
    if (g_touch_max_x <= g_touch_min_x) return v;
    int t = (v - g_touch_min_x) * g_screen_w / (g_touch_max_x - g_touch_min_x);
    if (t < 0) t = 0;
    if (t >= g_screen_w) t = g_screen_w - 1;
    return t;
}
static int map_y(int v) {
    if (!g_coord_mapped) return v;
    if (g_touch_max_y <= g_touch_min_y) return v;
    int t = (v - g_touch_min_y) * g_screen_h / (g_touch_max_y - g_touch_min_y);
    if (t < 0) t = 0;
    if (t >= g_screen_h) t = g_screen_h - 1;
    return t;
}

int input_init(const char *dev_path, int screen_w, int screen_h)
{
    const char *env_path = getenv("RK_GUI_INPUT");
    const char *path = (dev_path && dev_path[0]) ? dev_path :
                       (env_path && env_path[0]) ? env_path :
                       DEFAULT_INPUT_DEV;

    g_fd = open(path, O_RDONLY | O_NONBLOCK);
    if (g_fd < 0) {
        perror("[input] 打开输入设备失败");
        return -1;
    }

    /* 读取触摸范围 */
    struct input_absinfo ax, ay;
    if (ioctl(g_fd, EVIOCGABS(ABS_X), &ax) == 0) {
        g_touch_min_x = ax.minimum;
        g_touch_max_x = ax.maximum;
    } else {
        g_touch_min_x = 0; g_touch_max_x = 1023;
    }
    if (ioctl(g_fd, EVIOCGABS(ABS_Y), &ay) == 0) {
        g_touch_min_y = ay.minimum;
        g_touch_max_y = ay.maximum;
    } else {
        g_touch_min_y = 0; g_touch_max_y = 599;
    }

    /* 设置屏幕尺寸并判断是否需要映射 */
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    if (g_screen_w > 0 && g_screen_h > 0) {
        g_coord_mapped = 1;
    } else {
        g_coord_mapped = 0;
        g_screen_w = g_touch_max_x - g_touch_min_x + 1;
        g_screen_h = g_touch_max_y - g_touch_min_y + 1;
    }

    /* 注册信号处理 */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = input_signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    printf("[input] 已打开 %s, 触摸: X[%d..%d] Y[%d..%d]  屏幕: %dx%d %s\n",
           path, g_touch_min_x, g_touch_max_x, g_touch_min_y, g_touch_max_y,
           g_screen_w, g_screen_h,
           g_coord_mapped ? "(已启用坐标映射)" : "(原始坐标)");
    return 0;
}

void input_close(void)
{
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

int input_poll(input_event_t *out)
{
    if (!out) return -1;
    out->type = INPUT_NONE;
    out->x = 0;
    out->y = 0;

    if (g_quit) {
        out->type = INPUT_QUIT;
        return 0;
    }
    if (g_fd < 0) return -1;

    /* 状态: 跟踪按下-移动-抬起 */
    static int  s_down = 0;
    static int  s_start_x = 0, s_start_y = 0;
    static int  s_last_x = 0,  s_last_y = 0;
    static int  s_pend_press = 0, s_pend_release = 0;
    static int  s_swipe_fired = 0;   /* 一次按下中是否已经发出过 SWIPE */

    struct input_event ev;
    ssize_t n = read(g_fd, &ev, sizeof(ev));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;       /* 无数据 */
        }
        if (errno == EINTR) return 0;   /* 被信号打断, 不算错误 */
        perror("[input] read 失败");
        return -1;
    }
    if (n != (ssize_t)sizeof(ev)) {
        return 0;
    }

    /* 工具: 在 (sx,sy) 处判断是否需要触发 SWIPE, 触发则填 out 并返回 1 */
    #define CHECK_MOVE_SWIPE(sx, sy) do {                                      \
        if (s_down && !s_swipe_fired) {                                        \
            int _sx0 = map_x(s_start_x), _sy0 = map_y(s_start_y);              \
            int _sx1 = (sx), _sy1 = (sy);                                      \
            int _dx = _sx1 - _sx0, _dy = _sy1 - _sy0;                          \
            int _adx = abs_int(_dx), _ady = abs_int(_dy);                      \
            int _thr_x = g_screen_w / SWIPE_THRESHOLD_DIVISOR;                 \
            int _thr_y = g_screen_h / SWIPE_THRESHOLD_DIVISOR;                 \
            if (_thr_x < 1) _thr_x = 1;                                        \
            if (_thr_y < 1) _thr_y = 1;                                        \
            if (_adx > _thr_x || _ady > _thr_y) {                              \
                if (_adx >= _ady) {                                            \
                    out->type = (_dx > 0) ? INPUT_SWIPE_RIGHT                   \
                                          : INPUT_SWIPE_LEFT;                  \
                } else {                                                       \
                    out->type = (_dy > 0) ? INPUT_SWIPE_DOWN                   \
                                          : INPUT_SWIPE_UP;                    \
                }                                                              \
                out->x = _sx1;                                                 \
                out->y = _sy1;                                                 \
                s_swipe_fired = 1;                                             \
                return 0;                                                      \
            }                                                                  \
        }                                                                      \
    } while (0)

    switch (ev.type) {
    case EV_ABS:
        if (ev.code == ABS_X) {
            s_last_x = ev.value;
            CHECK_MOVE_SWIPE(map_x(s_last_x), map_y(s_last_y));
        } else if (ev.code == ABS_Y) {
            s_last_y = ev.value;
            CHECK_MOVE_SWIPE(map_x(s_last_x), map_y(s_last_y));
        }
        break;

    case EV_KEY:
        if (ev.code == BTN_TOUCH) {
            if (ev.value == 1)      s_pend_press   = 1;
            else if (ev.value == 0) s_pend_release = 1;
        } else if (ev.code == KEY_BACK && ev.value == 1) {
            out->type = INPUT_KEY_BACK;
            return 0;
        } else if (ev.code == KEY_POWER && ev.value == 1) {
            out->type = INPUT_KEY_POWER;
            return 0;
        }
        break;

    case EV_SYN:
        if (ev.code == SYN_REPORT) {
            if (s_pend_press) {
                s_start_x = s_last_x;
                s_start_y = s_last_y;
                s_down = 1;
                s_pend_press = 0;
                s_swipe_fired = 0;
            }
            /* MOVE 期间 (s_down && !s_pend_release) 也可能收到 SYN,
               如果当前点已满足阈值, 立即触发 SWIPE. */
            if (s_down && !s_pend_release) {
                CHECK_MOVE_SWIPE(map_x(s_last_x), map_y(s_last_y));
            }
            if (s_pend_release && s_down) {
                s_pend_release = 0;
                s_down = 0;

                /* 将触摸原始坐标映射到屏幕像素 */
                int sx0 = map_x(s_start_x), sy0 = map_y(s_start_y);
                int sx1 = map_x(s_last_x),  sy1 = map_y(s_last_y);

                int dx = sx1 - sx0;
                int dy = sy1 - sy0;
                int adx = abs_int(dx), ady = abs_int(dy);
                int thr_x = g_screen_w / SWIPE_THRESHOLD_DIVISOR;
                int thr_y = g_screen_h / SWIPE_THRESHOLD_DIVISOR;
                if (thr_x < 1) thr_x = 1;
                if (thr_y < 1) thr_y = 1;

                if (s_swipe_fired) {
                    /* 已在 MOVE 中触发过 SWIPE, release 时不再重复 */
                    return 0;
                }

                if (adx > thr_x || ady > thr_y) {
                    if (adx >= ady) {
                        out->type = (dx > 0) ? INPUT_SWIPE_RIGHT : INPUT_SWIPE_LEFT;
                    } else {
                        out->type = (dy > 0) ? INPUT_SWIPE_DOWN  : INPUT_SWIPE_UP;
                    }
                    out->x = sx1;
                    out->y = sy1;
                } else {
                    out->type = INPUT_TAP;
                    out->x = sx1;
                    out->y = sy1;
                }
                return 0;
            }
        }
        break;
    }
    #undef CHECK_MOVE_SWIPE
    return 0;
}

int input_poll_raw(input_event_t *out)
{
    if (!out) return -1;
    out->type = INPUT_NONE;
    out->x = 0;
    out->y = 0;

    if (g_quit) { out->type = INPUT_QUIT; return 0; }
    if (g_fd < 0) return -1;

    static int  s_rdown = 0;
    static int  s_rstart_x = 0, s_rstart_y = 0;
    static int  s_rlast_x = 0, s_rlast_y = 0;
    static int  s_r_pend_press = 0, s_r_pend_release = 0;

    struct input_event ev;
    ssize_t n = read(g_fd, &ev, sizeof(ev));
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        if (errno == EINTR) return 0;
        perror("[input] read 失败");
        return -1;
    }
    if (n != (ssize_t)sizeof(ev)) return 0;

    switch (ev.type) {
    case EV_ABS:
        if (ev.code == ABS_X) s_rlast_x = ev.value;
        else if (ev.code == ABS_Y) s_rlast_y = ev.value;
        break;
    case EV_KEY:
        if (ev.code == BTN_TOUCH) {
            if (ev.value == 1)      s_r_pend_press   = 1;
            else if (ev.value == 0) s_r_pend_release = 1;
        } else if (ev.code == KEY_BACK && ev.value == 1) {
            out->type = INPUT_KEY_BACK; return 0;
        } else if (ev.code == KEY_POWER && ev.value == 1) {
            out->type = INPUT_KEY_POWER; return 0;
        }
        break;
    case EV_SYN:
        if (ev.code == SYN_REPORT) {
            if (s_r_pend_press) {
                s_rdown = 1;
                s_rstart_x = s_rlast_x;
                s_rstart_y = s_rlast_y;
                s_r_pend_press = 0;
                out->type = INPUT_TOUCH_DOWN;
                out->x = map_x(s_rlast_x);
                out->y = map_y(s_rlast_y);
                return 0;
            }
            if (s_rdown && !s_r_pend_release) {
                /* 移动 (无 release 标记) */
                out->type = INPUT_TOUCH_MOVE;
                out->x = map_x(s_rlast_x);
                out->y = map_y(s_rlast_y);
                return 0;
            }
            if (s_r_pend_release) {
                s_r_pend_release = 0;
                s_rdown = 0;

                /* 计算从按下到抬起的总位移:
                 *   - 位移很小 -> 视为 TAP (用于点击按钮)
                 *   - 位移较大 -> 视为 TOUCH_UP (用于结束一笔)
                 */
                int sx0 = map_x(s_rstart_x), sy0 = map_y(s_rstart_y);
                int sx1 = map_x(s_rlast_x),  sy1 = map_y(s_rlast_y);
                int dx = sx1 - sx0, dy = sy1 - sy0;
                int thr_x = g_screen_w / SWIPE_THRESHOLD_DIVISOR;
                int thr_y = g_screen_h / SWIPE_THRESHOLD_DIVISOR;
                if (thr_x < 1) thr_x = 1;
                if (thr_y < 1) thr_y = 1;
                if (abs_int(dx) <= thr_x && abs_int(dy) <= thr_y) {
                    out->type = INPUT_TAP;
                } else {
                    out->type = INPUT_TOUCH_UP;
                }
                out->x = sx1;
                out->y = sy1;
                return 0;
            }
        }
        break;
    }
    return 0;
}

int input_wait_event(input_event_t *out, int timeout_ms)
{
    int poll_timeout;
    if (!out) return -1;
    if (g_fd < 0) return -1;

    /* 优先响应退出信号 */
    if (g_quit) {
        out->type = INPUT_QUIT;
        return 0;
    }

    /* API 约定: 0=永久等待, <0=非阻塞, >0=超时毫秒。 */
    poll_timeout = timeout_ms == 0 ? -1 : (timeout_ms < 0 ? 0 : timeout_ms);
    struct pollfd pfd = { .fd = g_fd, .events = POLLIN };
    int ret = poll(&pfd, 1, poll_timeout);
    if (ret < 0) {
        if (errno == EINTR) {
            if (g_quit) { out->type = INPUT_QUIT; return 0; }
            return -1;
        }
        perror("[input] poll 失败");
        return -1;
    }
    if (ret == 0) {
        /* 超时: 仍然检测一下退出信号 */
        if (g_quit) { out->type = INPUT_QUIT; return 0; }
        out->type = INPUT_NONE;
        return -1;       /* 超时返回 -1, 调用方继续循环 */
    }

    /* 有数据: 循环读取直到获取到有效事件 */
    for (;;) {
        ret = input_poll(out);
        if (ret < 0) return -1;
        if (out->type != INPUT_NONE) return 0;
        /* 短时间内若无更多数据, 退出 */
        struct pollfd pfd2 = { .fd = g_fd, .events = POLLIN };
        int r2 = poll(&pfd2, 1, 0);
        if (r2 <= 0) return -1;
    }
}
