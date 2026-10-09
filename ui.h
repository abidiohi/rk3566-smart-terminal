/* ============================================================================
 * 文件: ui.h
 * 说明: UI 基础绘制原语与按钮控件
 *      - 直接在 /dev/fb0 framebuffer 上绘制
 *      - 提供矩形/图片/简易文字(8x16位图字体)/按钮热区测试
 *      - 移植自参考代码 bmp_display, 扩展为支持区域绘制
 * ==========================================================================*/
#ifndef UI_H
#define UI_H

#include "bmp_display.h"   /* 复用 LCD/BMP 解析接口 */

/* 常用颜色 (ARGB 8888) */
#define UI_COLOR_BLACK   0xFF000000u
#define UI_COLOR_WHITE   0xFFFFFFFFu
#define UI_COLOR_RED     0xFFFF0000u
#define UI_COLOR_GREEN   0xFF00FF00u
#define UI_COLOR_BLUE    0xFF0000FFu
#define UI_COLOR_YELLOW  0xFFFFFF00u
#define UI_COLOR_GRAY    0xFF808080u
#define UI_COLOR_DGRAY   0xFF303030u
#define UI_COLOR_LGRAY   0xFFC0C0C0u
#define UI_COLOR_CYAN    0xFF00FFFFu

/* 屏幕尺寸缓存 (运行时通过 fb0 ioctl 探测) */
extern int g_screen_w;
extern int g_screen_h;

/* UI 初始化: 默认使用 /dev/fb0, 可由 RK_GUI_FB 覆盖。
 * 当前要求 32bpp, 并按驱动实际行跨度写入。 */
int  ui_init(void);

/* UI 清理: 解除 mmap, 关闭 fb0. 程序退出前调用一次, 保证资源释放. */
void ui_close(void);

/* 绘制一个填充矩形 (x, y, w, h), 颜色 ARGB */
int  ui_fill_rect(int x, int y, int w, int h, unsigned int color);

/* 绘制一个矩形边框 (线宽 1) */
int  ui_draw_rect(int x, int y, int w, int h, unsigned int color);

/* 绘制 BMP 图片到指定位置, path 为 24 位 BMP 文件路径
 *  - 缩放 scale <= 0 时按 1.0
 *  - 返回 0 成功, -1 失败
 */
int  ui_draw_bmp(const char *path, int x, int y, double scale);

/* 绘制 8x16 位图字体 ASCII 字符串 (仅 32..127)
 *  - scale 整数倍放大, 1=原大小, 2=2 倍...
 *  - bg 为背景色, fg 为前景色
 *  - 字体数据内置, 节省资源
 */
int  ui_draw_text(int x, int y, const char *str,
                  unsigned int fg, unsigned int bg, int scale);

/* 简易居中文本: 在矩形内水平/垂直居中绘制一行文字 */
int  ui_draw_text_center(int cx, int cy, const char *str,
                         unsigned int fg, unsigned int bg, int scale);

/* 按钮结构 */
typedef struct {
    int x, y, w, h;
    const char *label;        /* 按钮文字 (可选) */
    int id;                   /* 业务侧自定义 ID */
} ui_button_t;

/* 判断 (x, y) 是否落在按钮区域内, 是则返回 1, 否则 0 */
int  ui_button_hit(const ui_button_t *btn, int x, int y);

/* 绘制按钮: 圆角矩形 + 文字 (label 可为 NULL)。
 * text_bg 为兼容保留, 实际使用 fill 以避免字形背景色块。 */
int  ui_draw_button(const ui_button_t *btn,
                    unsigned int fill, unsigned int border,
                    unsigned int text_fg, unsigned int text_bg, int text_scale);

/* 绘制按钮按下态 (颜色变深) */
int  ui_draw_button_pressed(const ui_button_t *btn);

/* 全屏清屏 */
int  ui_clear(unsigned int color);

/* 单像素写入 (越界自动忽略) */
int  ui_put_pixel(int x, int y, unsigned int color);

/* 填充圆 (中点圆算法, 含边界) */
int  ui_fill_circle(int cx, int cy, int r, unsigned int color);

/* 画一条水平线 */
int  ui_draw_hline(int x, int y, int w, unsigned int color);

/* 画一条垂直线 */
int  ui_draw_vline(int x, int y, int h, unsigned int color);

/* 任意方向直线 (Bresenham), 端点均包含 */
int  ui_draw_line(int x0, int y0, int x1, int y1, unsigned int color);

/* 圆形轮廓 */
int  ui_draw_circle(int cx, int cy, int r, unsigned int color);

/* 圆角矩形。radius 会自动限制到短边的一半。 */
int  ui_fill_round_rect(int x, int y, int w, int h, int radius,
                        unsigned int color);
int  ui_draw_round_rect(int x, int y, int w, int h, int radius,
                        unsigned int color);

#endif /* UI_H */
