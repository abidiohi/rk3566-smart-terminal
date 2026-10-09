/* ============================================================================
 * 文件: touch_ctrl.h
 * 说明: 虚拟触控按键组件
 *      - 为游戏提供半透明虚拟按键 (方向键 / 动作键)
 *      - 支持点击和长按连续触发
 *      - 自适应屏幕尺寸布局
 *      - 与五子棋底部按钮风格协调
 * ==========================================================================*/
#ifndef TOUCH_CTRL_H
#define TOUCH_CTRL_H

/* 虚拟按键类型 */
typedef enum {
    VKEY_UP    = 0,
    VKEY_DOWN  = 1,
    VKEY_LEFT  = 2,
    VKEY_RIGHT = 3,
    VKEY_A,          /* 动作键 A */
    VKEY_B,          /* 动作键 B */
    VKEY_PAUSE,      /* 暂停 */
    VKEY_COUNT
} vkey_t;

/* 虚拟按键布局预设 */
typedef enum {
    VLAYOUT_DPAD_ONLY,       /* 仅方向十字键 */
    VLAYOUT_DPAD_ACTIONS,    /* 方向键 + 动作键 */
    VLAYOUT_HORIZONTAL,      /* 左右键水平排列 (弹球) */
} vlayout_t;

/* 初始化虚拟按键
 *  layout: 布局预设
 *  bottom_reserve: 底部保留区域高度 (px), 用于避免与底部按钮栏重叠
 *  根据屏幕尺寸自动计算按键位置和大小
 */
void touch_ctrl_init(vlayout_t layout, int bottom_reserve);

/* 绘制所有虚拟按键到 framebuffer (半透明叠加) */
void touch_ctrl_draw(void);

/* 检测触摸坐标 (screen_x, screen_y) 命中哪个虚拟键
 *  返回 VKEY_COUNT 表示没有命中
 */
vkey_t touch_ctrl_hit(int x, int y);

/* 擦除虚拟按键区域 (用背景色填充)
 *  background_color: 游戏区域背景色, 用于擦除
 */
void touch_ctrl_erase(unsigned int background_color);

#endif /* TOUCH_CTRL_H */
