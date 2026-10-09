/* ============================================================================
 * 文件: main_ui.h
 * 说明: 系统主界面
 *      - 顶部状态栏: 时间/日期
 *      - 单屏应用区 + 系统功能侧栏, 避免分页寻找
 *      - 滑动可移动焦点, 点击直接打开
 *      - 点击按钮返回按钮 id, 业务层据此跳转到对应子界面
 * ==========================================================================*/
#ifndef MAIN_UI_H
#define MAIN_UI_H

/* 主界面按钮 ID (业务侧) */
enum {
    MAIN_BTN_NONE = 0,
    /* Page 1: 多媒体/工具 */
    MAIN_BTN_ALBUM,        /* 相册 */
    MAIN_BTN_PAINT,        /* 画板 */
    MAIN_BTN_BALL,         /* 板接球 */
    MAIN_BTN_SNAKE,        /* 贪吃蛇 */
    /* Page 2: 其他 */
    MAIN_BTN_GOMOKU,       /* 五子棋 */
    MAIN_BTN_SET_PASS,     /* 设置密码 (原 Login) */
    MAIN_BTN_ABOUT,        /* 关于 */
    MAIN_BTN_POWER,        /* 关机 */
    /* 兼容: 旧版预留按钮 (Music/Video/Settings) 已从主界面移除 */
    MAIN_BTN_MUSIC = MAIN_BTN_NONE,
    MAIN_BTN_VIDEO = MAIN_BTN_NONE,
    MAIN_BTN_SETTINGS = MAIN_BTN_NONE,
    MAIN_BTN_LOGIN = MAIN_BTN_SET_PASS,
    MAIN_BTN_MAX
};

/* 绘制主界面 (背景 + 状态栏 + 按钮) */
int  main_ui_draw(void);

/* 内部事件循环:
 *  - 轮询触摸事件
 *  - 点击按钮高亮 + 触发返回
 *  - 四向滑动循环移动焦点
 *  - 电源键直接触发 Power
 *  返回: 被点击的按钮 ID, 0 = 取消(超时或退出信号)
 *
 *  调用方负责在退出后决定跳转到哪个子界面.
 */
int  main_ui_run(void);

/* 单独绘制一次状态栏(供其他界面复用) */
int  main_ui_draw_statusbar(void);

#endif /* MAIN_UI_H */
