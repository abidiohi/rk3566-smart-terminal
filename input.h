/* ============================================================================
 * 文件: input.h
 * 说明: 触摸/键盘输入事件抽象层
 *      将 /dev/input/event6 的原始事件封装为语义事件:
 *      点击(TAP)、上滑/下滑/左滑/右滑(SWIPE_*)、退出(QUIT)等
 *      RK3566 默认触摸屏设备节点为 /dev/input/event6
 * ==========================================================================*/
#ifndef INPUT_H
#define INPUT_H

/* 输入事件类型 */
typedef enum {
    INPUT_NONE = 0,        /* 无事件 */
    INPUT_TAP,             /* 点击 (x, y) 有效 */
    INPUT_SWIPE_LEFT,      /* 左滑 (右→左) */
    INPUT_SWIPE_RIGHT,     /* 右滑 (左→右) */
    INPUT_SWIPE_UP,        /* 上滑 (下→上) */
    INPUT_SWIPE_DOWN,      /* 下滑 (上→下) */
    INPUT_TOUCH_DOWN,      /* 触摸按下 (x, y 起点) - 画板等需要连续输入的模块 */
    INPUT_TOUCH_MOVE,      /* 触摸滑动 (x, y 当前) */
    INPUT_TOUCH_UP,        /* 触摸抬起 (x, y 终点) */
    INPUT_KEY_BACK,        /* 返回键(预留) */
    INPUT_KEY_POWER,       /* 电源键(预留) */
    INPUT_QUIT             /* 收到退出信号 */
} input_event_type_t;

/* 输入事件结构 */
typedef struct {
    input_event_type_t type;
    int x;                 /* 点击 x 坐标 (SWIPE 时为结束坐标) */
    int y;                 /* 点击 y 坐标 */
} input_event_t;

/* 初始化输入子系统
 *  - dev_path: 输入设备路径; NULL 或 "" 时先读取 RK_GUI_INPUT,
 *              未设置则使用默认 /dev/input/event6
 *  - screen_w, screen_h: 屏幕尺寸, 用于将触摸坐标映射到屏幕坐标
 *                        传 0 则使用原始触摸坐标(不推荐)
 *  返回: 0 成功, -1 失败
 */
int  input_init(const char *dev_path, int screen_w, int screen_h);

/* 关闭输入子系统, 释放资源 */
void input_close(void);

/* 非阻塞轮询一个事件
 *  - out: 输出事件
 *  返回: 0 获取到事件(可能是 INPUT_NONE), -1 设备错误
 */
int  input_poll(input_event_t *out);

/* 非阻塞轮询: 返回原始触摸事件 (DOWN/MOVE/UP) 以及短按识别 (TAP)
 *  - 供画板等需要连续输入轨迹的模块使用
 *  - 与 input_poll 互斥: 同时只能用一个, 切换前需清空状态
 *  - 若按下后总位移 < 屏宽/SWIPE_THRESHOLD_DIVISOR, 释放时上报 TAP;
 *    否则上报 TOUCH_UP.
 *  返回: 0 获取到事件(可能是 INPUT_NONE), -1 设备错误
 */
int  input_poll_raw(input_event_t *out);

/* 阻塞等待一个有效事件(忽略 INPUT_NONE)
 *  - timeout_ms: 超时时间(毫秒), 0 = 永久等待, <0 = 非阻塞
 *  返回: 0 获取到事件, -1 超时或错误
 *
 *  调用方在循环中处理事件, 遇到 INPUT_QUIT 时退出主循环
 */
int  input_wait_event(input_event_t *out, int timeout_ms);

#endif /* INPUT_H */
