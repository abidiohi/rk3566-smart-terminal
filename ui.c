/* ============================================================================
 * 文件: ui.c
 * 说明: UI 基础绘制原语实现
 *      全部基于 /dev/fb0 framebuffer, 内部缓存 mmap 避免每帧重映射.
 * ==========================================================================*/
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

/* 全局缓存 */
int g_screen_w = 0;
int g_screen_h = 0;
static int           g_fb_fd   = -1;
static unsigned int *g_fb      = NULL;
static unsigned int  g_fb_size = 0;
static int           g_fb_stride = 0;
static int           g_fb_xoffset = 0;
static int           g_fb_yoffset = 0;

/* ---------- 私有: 写一个像素 ---------- */
static inline void put_pixel(int x, int y, unsigned int color)
{
    if (x < 0 || y < 0 || x >= g_screen_w || y >= g_screen_h) return;
    g_fb[(y + g_fb_yoffset) * g_fb_stride + x + g_fb_xoffset] = color;
}

/* ---------- 初始化 ---------- */
int ui_init(void)
{
    if (g_fb_fd >= 0) return 0;     /* 已初始化 */

    const char *fb_path = getenv("RK_GUI_FB");
    if (!fb_path || !fb_path[0]) fb_path = "/dev/fb0";
    g_fb_fd = open(fb_path, O_RDWR);
    if (g_fb_fd < 0) {
        perror("[ui] 打开 framebuffer 失败");
        return -1;
    }
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        perror("[ui] 获取屏幕信息失败");
        close(g_fb_fd);
        g_fb_fd = -1;
        return -1;
    }
    g_screen_w  = vinfo.xres;
    g_screen_h  = vinfo.yres;
    if (vinfo.bits_per_pixel != 32 || finfo.line_length < vinfo.xres * 4) {
        fprintf(stderr, "[ui] 仅支持 32bpp framebuffer (当前 %ubpp, stride=%u)\n",
                vinfo.bits_per_pixel, finfo.line_length);
        close(g_fb_fd);
        g_fb_fd = -1;
        return -1;
    }
    g_fb_stride = (int)(finfo.line_length / sizeof(unsigned int));
    g_fb_xoffset = (int)vinfo.xoffset;
    g_fb_yoffset = (int)vinfo.yoffset;
    g_fb_size   = finfo.smem_len;
    g_fb = (unsigned int *)mmap(NULL, g_fb_size,
                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                g_fb_fd, 0);
    if (g_fb == MAP_FAILED) {
        perror("[ui] mmap 失败");
        g_fb = NULL;
        close(g_fb_fd);
        g_fb_fd = -1;
        return -1;
    }
    printf("[ui] %s: %dx%d stride=%d size=%u bytes\n",
           fb_path, g_screen_w, g_screen_h, g_fb_stride, g_fb_size);
    return 0;
}

/* ---------- 清理 ---------- */
void ui_close(void)
{
    if (g_fb && g_fb != MAP_FAILED) {
        munmap(g_fb, g_fb_size);
        g_fb = NULL;
        g_fb_stride = 0;
        g_fb_xoffset = 0;
        g_fb_yoffset = 0;
    }
    if (g_fb_fd >= 0) {
        close(g_fb_fd);
        g_fb_fd = -1;
    }
}

/* ---------- 清屏 ---------- */
int ui_clear(unsigned int color)
{
    if (!g_fb) return -1;
    for (int y = 0; y < g_screen_h; y++) {
        unsigned int *row = g_fb + (y + g_fb_yoffset) * g_fb_stride
                           + g_fb_xoffset;
        for (int x = 0; x < g_screen_w; x++) row[x] = color;
    }
    return 0;
}

/* ---------- 填充矩形 ---------- */
int ui_fill_rect(int x, int y, int w, int h, unsigned int color)
{
    if (!g_fb || w <= 0 || h <= 0) return -1;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g_screen_w) x1 = g_screen_w;
    if (y1 > g_screen_h) y1 = g_screen_h;
    for (int yy = y0; yy < y1; yy++) {
        unsigned int *p = g_fb + (yy + g_fb_yoffset) * g_fb_stride
                          + x0 + g_fb_xoffset;
        for (int xx = x0; xx < x1; xx++) *p++ = color;
    }
    return 0;
}

/* ---------- 矩形边框 ---------- */
int ui_draw_rect(int x, int y, int w, int h, unsigned int color)
{
    if (!g_fb || w <= 0 || h <= 0) return -1;
    int x0 = x, y0 = y, x1 = x + w - 1, y1 = y + h - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= g_screen_w) x1 = g_screen_w - 1;
    if (y1 >= g_screen_h) y1 = g_screen_h - 1;
    for (int xx = x0; xx <= x1; xx++) {
        put_pixel(xx, y0, color);
        put_pixel(xx, y1, color);
    }
    for (int yy = y0; yy <= y1; yy++) {
        put_pixel(x0, yy, color);
        put_pixel(x1, yy, color);
    }
    return 0;
}

/* ---------- 绘制 BMP 图片 ----------
 * 内部直接写 fb0, 避免每次 open/mmap, 性能更佳.
 * 兼容 BMP 自底向上(自动 flip).
 */
int ui_draw_bmp(const char *path, int x, int y, double scale)
{
    if (!g_fb) return -1;
    bmp_info_t info;
    if (read_bmp_info(path, &info) < 0) return -1;
    if (info.bpp != 24) {
        fprintf(stderr, "[ui] 不支持的位深: %d\n", info.bpp);
        return -1;
    }
    unsigned char *pixels = read_bmp_pixels(path, &info);
    if (!pixels) return -1;

    /* 是否需要缩放 */
    unsigned char *src = pixels;
    unsigned char *scaled = NULL;
    unsigned int dw = info.width, dh = info.height;
    unsigned int src_row_sz = info.row_size;
    if (scale > 0.0 && (scale < 0.999 || scale > 1.001)) {
        scaled = scale_bmp_nearest(src, info.width, info.height,
                                   src_row_sz, scale, &dw, &dh);
        if (scaled) {
            src = scaled;
            src_row_sz = dw * 3;
        }
    }

    /* 逐像素绘制 (BGR -> ARGB) */
    for (unsigned int dy = 0; dy < dh; dy++) {
        int sy = y + (int)dy;
        if (sy < 0 || sy >= g_screen_h) continue;
        /* 缩放后的数据本身已按比例排列,直接用 dy 寻址;
         * 仅在是否自底向上时翻转到目标行 */
        int src_row;
        if (scaled) {
            src_row = info.bottom_up ? ((int)dh - 1 - (int)dy) : (int)dy;
        } else {
            src_row = info.bottom_up ? ((int)info.height - 1 - (int)dy)
                                     : (int)dy;
        }
        const unsigned char *row = src + src_row * src_row_sz;
        unsigned int *dst = g_fb + (sy + g_fb_yoffset) * g_fb_stride
                            + g_fb_xoffset + (x < 0 ? 0 : x);
        int dx_start = (x < 0) ? -x : 0;
        int dx_end   = (int)dw;
        if (x + (int)dw > g_screen_w) dx_end = g_screen_w - x;
        for (int dx = dx_start; dx < dx_end; dx++) {
            unsigned int color = 0xFF000000u
                               | ((unsigned int)row[dx * 3 + 2] << 16)
                               | ((unsigned int)row[dx * 3 + 1] << 8)
                               |  (unsigned int)row[dx * 3 + 0];
            dst[dx] = color;
        }
    }

    if (scaled) free(scaled);
    free(pixels);
    return 0;
}

/* ---------- 8x16 ASCII 位图字体 ----------
 * 字体数据来自经典的 8x16 PC BIOS 风格, 这里取前 96 个可打印字符
 * (空格 0x20 到 '~' 0x7E). 每个字符 16 字节, 每字节表示一行 8 个像素.
 */
static const unsigned char font8x16[96][16] = {
    /* 0x20 ' ' */ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x21 '!' */ {0,0x18,0x3C,0x3C,0x18,0x18,0,0x18,0x18,0,0,0,0,0,0,0},
    /* 0x22 '"' */ {0,0x66,0x66,0x66,0x24,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x23 '#' */ {0,0x00,0x00,0x6C,0x6C,0xFE,0x6C,0x6C,0x6C,0xFE,0x6C,0x6C,0,0,0,0},
    /* 0x24 '$' */ {0x18,0x18,0x7C,0xC6,0xC2,0xC0,0x7C,0x06,0x06,0x86,0xC6,0x7C,0x18,0x18,0,0},
    /* 0x25 '%'*/ {0,0,0,0,0xC2,0xC6,0x0C,0x18,0x30,0x60,0xC6,0x86,0,0,0,0},
    /* 0x26 '&' */ {0,0,0x38,0x6C,0x6C,0x38,0x76,0xDC,0xCC,0xCC,0xCC,0x76,0,0,0,0},
    /* 0x27 ''' */ {0,0x30,0x30,0x30,0x60,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x28 '(' */ {0,0,0x0C,0x18,0x30,0x30,0x30,0x30,0x30,0x30,0x18,0x0C,0,0,0,0},
    /* 0x29 ')' */ {0,0,0x30,0x18,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x18,0x30,0,0,0,0},
    /* 0x2A '*' */ {0,0,0,0,0,0x66,0x3C,0xFF,0x3C,0x66,0,0,0,0,0,0},
    /* 0x2B '+' */ {0,0,0,0,0,0x18,0x18,0x7E,0x18,0x18,0,0,0,0,0,0},
    /* 0x2C ',' */ {0,0,0,0,0,0,0,0,0,0x18,0x18,0x18,0x30,0,0,0},
    /* 0x2D '-' */ {0,0,0,0,0,0,0,0xFE,0,0,0,0,0,0,0,0},
    /* 0x2E '.' */ {0,0,0,0,0,0,0,0,0,0,0,0x18,0x18,0,0,0},
    /* 0x2F '/' */ {0,0,0,0,0x02,0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0,0,0,0},
    /* 0x30 '0' */ {0,0,0x3C,0x66,0xC3,0xC3,0xDB,0xDB,0xC3,0xC3,0x66,0x3C,0,0,0,0},
    /* 0x31 '1' */ {0,0,0x18,0x38,0x78,0x18,0x18,0x18,0x18,0x18,0x18,0x7E,0,0,0,0},
    /* 0x32 '2' */ {0,0,0x7C,0xC6,0x06,0x0C,0x18,0x30,0x60,0xC0,0xC6,0xFE,0,0,0,0},
    /* 0x33 '3' */ {0,0,0x7C,0xC6,0x06,0x06,0x3C,0x06,0x06,0x06,0xC6,0x7C,0,0,0,0},
    /* 0x34 '4' */ {0,0,0x0C,0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x0C,0x0C,0x1E,0,0,0,0},
    /* 0x35 '5' */ {0,0,0xFE,0xC0,0xC0,0xC0,0xFC,0x06,0x06,0x06,0xC6,0x7C,0,0,0,0},
    /* 0x36 '6' */ {0,0,0x38,0x60,0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x7C,0,0,0,0},
    /* 0x37 '7' */ {0,0,0xFE,0xC6,0x06,0x06,0x0C,0x18,0x30,0x30,0x30,0x30,0,0,0,0},
    /* 0x38 '8' */ {0,0,0x7C,0xC6,0xC6,0xC6,0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0,0,0,0},
    /* 0x39 '9' */ {0,0,0x7C,0xC6,0xC6,0xC6,0x7E,0x06,0x06,0x06,0x0C,0x78,0,0,0,0},
    /* 0x3A ':' */ {0,0,0,0,0x18,0x18,0,0,0,0x18,0x18,0,0,0,0,0},
    /* 0x3B ';' */ {0,0,0,0,0x18,0x18,0,0,0,0x18,0x18,0x30,0,0,0,0},
    /* 0x3C '<' */ {0,0,0,0x06,0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x06,0,0,0,0},
    /* 0x3D '=' */ {0,0,0,0,0,0,0x7E,0,0x7E,0,0,0,0,0,0,0},
    /* 0x3E '>' */ {0,0,0,0,0x60,0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0,0,0,0},
    /* 0x3F '?' */ {0,0,0x7C,0xC6,0xC6,0x0C,0x18,0x18,0x18,0,0x18,0x18,0,0,0,0},
    /* 0x40 '@' */ {0,0,0x00,0x00,0x00,0x7C,0xC6,0xC6,0xDE,0xDE,0xDE,0xDC,0xC0,0x7C,0,0},
    /* 0x41 'A' */ {0,0,0x10,0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0,0,0,0},
    /* 0x42 'B' */ {0,0,0xFC,0x66,0x66,0x66,0x7C,0x66,0x66,0x66,0x66,0xFC,0,0,0,0},
    /* 0x43 'C' */ {0,0,0x3C,0x66,0xC2,0xC0,0xC0,0xC0,0xC0,0xC2,0x66,0x3C,0,0,0,0},
    /* 0x44 'D' */ {0,0,0xF8,0x6C,0x66,0x66,0x66,0x66,0x66,0x66,0x6C,0xF8,0,0,0,0},
    /* 0x45 'E' */ {0,0,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x62,0x66,0xFE,0,0,0,0},
    /* 0x46 'F' */ {0,0,0xFE,0x66,0x62,0x68,0x78,0x68,0x60,0x60,0x60,0xF0,0,0,0,0},
    /* 0x47 'G' */ {0,0,0x3C,0x66,0xC2,0xC0,0xC0,0xDE,0xC6,0xC6,0x66,0x3A,0,0,0,0},
    /* 0x48 'H' */ {0,0,0xC6,0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0xC6,0xC6,0,0,0,0},
    /* 0x49 'I' */ {0,0,0x3C,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0},
    /* 0x4A 'J' */ {0,0,0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0xCC,0xCC,0xCC,0x78,0,0,0,0},
    /* 0x4B 'K' */ {0,0,0xE6,0x66,0x66,0x6C,0x78,0x78,0x6C,0x66,0x66,0xE6,0,0,0,0},
    /* 0x4C 'L' */ {0,0,0xF0,0x60,0x60,0x60,0x60,0x60,0x60,0x62,0x66,0xFE,0,0,0,0},
    /* 0x4D 'M' */ {0,0,0xC6,0xEE,0xFE,0xFE,0xD6,0xC6,0xC6,0xC6,0xC6,0xC6,0,0,0,0},
    /* 0x4E 'N' */ {0,0,0xC6,0xE6,0xF6,0xFE,0xDE,0xCE,0xC6,0xC6,0xC6,0xC6,0,0,0,0},
    /* 0x4F 'O' */ {0,0,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0,0,0,0},
    /* 0x50 'P' */ {0,0,0xFC,0x66,0x66,0x66,0x7C,0x60,0x60,0x60,0x60,0xF0,0,0,0,0},
    /* 0x51 'Q' */ {0,0,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x0C,0x0E,0,0},
    /* 0x52 'R' */ {0,0,0xFC,0x66,0x66,0x66,0x7C,0x6C,0x66,0x66,0x66,0xE6,0,0,0,0},
    /* 0x53 'S' */ {0,0,0x7C,0xC6,0xC6,0x60,0x38,0x0C,0x06,0xC6,0xC6,0x7C,0,0,0,0},
    /* 0x54 'T' */ {0,0,0xFF,0xDB,0x99,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0},
    /* 0x55 'U' */ {0,0,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0,0,0,0},
    /* 0x56 'V' */ {0,0,0xC3,0xC3,0xC3,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x18,0,0,0,0},
    /* 0x57 'W' */ {0,0,0xC6,0xC6,0xC6,0xC6,0xC6,0xD6,0xD6,0xFE,0x6C,0x6C,0,0,0,0},
    /* 0x58 'X' */ {0,0,0xC3,0xC3,0x66,0x3C,0x18,0x18,0x3C,0x66,0xC3,0xC3,0,0,0,0},
    /* 0x59 'Y' */ {0,0,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x18,0x18,0x18,0x3C,0,0,0,0},
    /* 0x5A 'Z' */ {0,0,0xFE,0xC6,0x86,0x0C,0x18,0x30,0x60,0xC2,0xC6,0xFE,0,0,0,0},
    /* 0x5B '[' */ {0,0,0x3C,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x3C,0,0,0,0},
    /* 0x5C '\' */ {0,0,0,0,0x80,0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0,0,0,0},
    /* 0x5D ']' */ {0,0,0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0,0,0,0},
    /* 0x5E '^' */ {0x10,0x38,0x6C,0xC6,0,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x5F '_' */ {0,0,0,0,0,0,0,0,0,0,0,0,0xFF,0,0,0},
    /* 0x60 '`' */ {0x30,0x30,0x18,0,0,0,0,0,0,0,0,0,0,0,0,0},
    /* 0x61 'a' */ {0,0,0,0,0,0x78,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0,0,0,0},
    /* 0x62 'b' */ {0,0,0xE0,0x60,0x60,0x78,0x6C,0x66,0x66,0x66,0x66,0x7C,0,0,0,0},
    /* 0x63 'c' */ {0,0,0,0,0,0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0,0,0,0},
    /* 0x64 'd' */ {0,0,0x1C,0x0C,0x0C,0x3C,0x6C,0xCC,0xCC,0xCC,0xCC,0x76,0,0,0,0},
    /* 0x65 'e' */ {0,0,0,0,0,0x7C,0xC6,0xFE,0xC0,0xC0,0xC6,0x7C,0,0,0,0},
    /* 0x66 'f' */ {0,0,0x38,0x6C,0x64,0x60,0xF0,0x60,0x60,0x60,0x60,0xF0,0,0,0,0},
    /* 0x67 'g' */ {0,0,0,0,0,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0xCC,0x78,0},
    /* 0x68 'h' */ {0,0,0xE0,0x60,0x60,0x6C,0x76,0x66,0x66,0x66,0x66,0xE6,0,0,0,0},
    /* 0x69 'i' */ {0,0,0x18,0x18,0,0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0},
    /* 0x6A 'j' */ {0,0,0x06,0x06,0,0x0E,0x06,0x06,0x06,0x06,0x06,0x06,0x66,0x66,0x3C,0},
    /* 0x6B 'k' */ {0,0,0xE0,0x60,0x60,0x66,0x6C,0x78,0x78,0x6C,0x66,0xE6,0,0,0,0},
    /* 0x6C 'l' */ {0,0,0x38,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x3C,0,0,0,0},
    /* 0x6D 'm' */ {0,0,0,0,0,0xEC,0xFE,0xD6,0xD6,0xD6,0xD6,0xC6,0,0,0,0},
    /* 0x6E 'n' */ {0,0,0,0,0,0xDC,0x66,0x66,0x66,0x66,0x66,0x66,0,0,0,0},
    /* 0x6F 'o' */ {0,0,0,0,0,0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0,0,0,0},
    /* 0x70 'p' */ {0,0,0,0,0,0xDC,0x66,0x66,0x66,0x66,0x66,0x7C,0x60,0x60,0xF0,0},
    /* 0x71 'q' */ {0,0,0,0,0,0x76,0xCC,0xCC,0xCC,0xCC,0xCC,0x7C,0x0C,0x0C,0x1E,0},
    /* 0x72 'r' */ {0,0,0,0,0,0xDC,0x76,0x66,0x60,0x60,0x60,0xF0,0,0,0,0},
    /* 0x73 's' */ {0,0,0,0,0,0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0,0,0,0},
    /* 0x74 't' */ {0,0,0x20,0x60,0x60,0xFC,0x60,0x60,0x60,0x60,0x66,0x3C,0,0,0,0},
    /* 0x75 'u' */ {0,0,0,0,0,0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0,0,0,0},
    /* 0x76 'v' */ {0,0,0,0,0,0xC3,0xC3,0xC3,0x66,0x3C,0x18,0x18,0,0,0,0},
    /* 0x77 'w' */ {0,0,0,0,0,0xC6,0xC6,0xD6,0xD6,0xD6,0xFE,0x6C,0,0,0,0},
    /* 0x78 'x' */ {0,0,0,0,0,0xC6,0x6C,0x38,0x38,0x38,0x6C,0xC6,0,0,0,0},
    /* 0x79 'y' */ {0,0,0,0,0,0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x0C,0xCC,0x78,0},
    /* 0x7A 'z' */ {0,0,0,0,0,0xFE,0xCC,0x18,0x30,0x60,0xC6,0xFE,0,0,0,0},
    /* 0x7B '{' */ {0,0,0,0x0E,0x18,0x18,0x18,0x70,0x18,0x18,0x18,0x0E,0,0,0,0},
    /* 0x7C '|' */ {0,0,0,0x18,0x18,0x18,0,0x18,0x18,0x18,0,0,0,0,0,0},
    /* 0x7D '}' */ {0,0,0,0x70,0x18,0x18,0x18,0x0E,0x18,0x18,0x18,0x70,0,0,0,0},
    /* 0x7E '~' */ {0,0,0,0x76,0xDC,0,0,0,0,0,0,0,0,0,0,0}
};

int ui_draw_text(int x, int y, const char *str,
                 unsigned int fg, unsigned int bg, int scale)
{
    if (!g_fb || !str) return -1;
    if (scale < 1) scale = 1;

    int cx = x, cy = y;
    for (const char *p = str; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch < 0x20 || ch > 0x7E) ch = '?';
        const unsigned char *glyph = font8x16[ch - 0x20];

        for (int row = 0; row < 16; row++) {
            unsigned char bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                int draw = (bits >> (7 - col)) & 1;
                unsigned int color = draw ? fg : bg;
                if (bg == 0xFF000000u) {
                    /* bg=全透明黑: 透明像素不绘制, 仅画前景 */
                    if (!draw) continue;
                }
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        put_pixel(cx + col * scale + sx,
                                  cy + row * scale + sy, color);
                    }
                }
            }
        }
        cx += 8 * scale;
    }
    return 0;
}

int ui_draw_text_center(int cx, int cy, const char *str,
                        unsigned int fg, unsigned int bg, int scale)
{
    if (!str) return -1;
    if (scale < 1) scale = 1;
    int len = (int)strlen(str);
    int w = len * 8 * scale;
    int h = 16 * scale;
    return ui_draw_text(cx - w / 2, cy - h / 2, str, fg, bg, scale);
}

/* ---------- 单像素写入 ---------- */
int ui_put_pixel(int x, int y, unsigned int color)
{
    if (!g_fb) return -1;
    if (x < 0 || y < 0 || x >= g_screen_w || y >= g_screen_h) return 0;
    g_fb[(y + g_fb_yoffset) * g_fb_stride + x + g_fb_xoffset] = color;
    return 0;
}

/* ---------- 填充圆 ---------- */
int ui_fill_circle(int cx, int cy, int r, unsigned int color)
{
    if (!g_fb || r <= 0) return -1;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx * dx + dy * dy <= r * r) {
                ui_put_pixel(cx + dx, cy + dy, color);
            }
        }
    }
    return 0;
}

/* ---------- 水平线 / 垂直线 ---------- */
int ui_draw_hline(int x, int y, int w, unsigned int color)
{
    if (!g_fb) return -1;
    if (w <= 0) return 0;
    int x0 = x, x1 = x + w;
    if (x0 < 0) x0 = 0;
    if (x1 > g_screen_w) x1 = g_screen_w;
    if (y < 0 || y >= g_screen_h) return 0;
    for (int xx = x0; xx < x1; xx++)
        g_fb[(y + g_fb_yoffset) * g_fb_stride + xx + g_fb_xoffset] = color;
    return 0;
}

int ui_draw_vline(int x, int y, int h, unsigned int color)
{
    if (!g_fb) return -1;
    if (h <= 0) return 0;
    int y0 = y, y1 = y + h;
    if (y0 < 0) y0 = 0;
    if (y1 > g_screen_h) y1 = g_screen_h;
    if (x < 0 || x >= g_screen_w) return 0;
    for (int yy = y0; yy < y1; yy++)
        g_fb[(yy + g_fb_yoffset) * g_fb_stride + x + g_fb_xoffset] = color;
    return 0;
}

int ui_draw_line(int x0, int y0, int x1, int y1, unsigned int color)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = err * 2;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    return 0;
}

int ui_draw_circle(int cx, int cy, int r, unsigned int color)
{
    int x = r;
    int y = 0;
    int err = 1 - r;
    if (!g_fb || r <= 0) return -1;
    while (x >= y) {
        put_pixel(cx + x, cy + y, color);
        put_pixel(cx + y, cy + x, color);
        put_pixel(cx - y, cy + x, color);
        put_pixel(cx - x, cy + y, color);
        put_pixel(cx - x, cy - y, color);
        put_pixel(cx - y, cy - x, color);
        put_pixel(cx + y, cy - x, color);
        put_pixel(cx + x, cy - y, color);
        y++;
        if (err < 0) err += 2 * y + 1;
        else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
    return 0;
}

int ui_fill_round_rect(int x, int y, int w, int h, int radius,
                       unsigned int color)
{
    if (!g_fb || w <= 0 || h <= 0) return -1;
    if (radius < 0) radius = 0;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    if (radius == 0) return ui_fill_rect(x, y, w, h, color);

    ui_fill_rect(x + radius, y, w - 2 * radius, h, color);
    ui_fill_rect(x, y + radius, w, h - 2 * radius, color);
    ui_fill_circle(x + radius, y + radius, radius, color);
    ui_fill_circle(x + w - radius - 1, y + radius, radius, color);
    ui_fill_circle(x + radius, y + h - radius - 1, radius, color);
    ui_fill_circle(x + w - radius - 1, y + h - radius - 1, radius, color);
    return 0;
}

int ui_draw_round_rect(int x, int y, int w, int h, int radius,
                       unsigned int color)
{
    int cx0, cx1, cy0, cy1;
    int px, py, err;
    if (!g_fb || w <= 0 || h <= 0) return -1;
    if (radius < 0) radius = 0;
    if (radius * 2 > w) radius = w / 2;
    if (radius * 2 > h) radius = h / 2;
    if (radius == 0) return ui_draw_rect(x, y, w, h, color);

    ui_draw_hline(x + radius, y, w - 2 * radius, color);
    ui_draw_hline(x + radius, y + h - 1, w - 2 * radius, color);
    ui_draw_vline(x, y + radius, h - 2 * radius, color);
    ui_draw_vline(x + w - 1, y + radius, h - 2 * radius, color);

    cx0 = x + radius;
    cx1 = x + w - radius - 1;
    cy0 = y + radius;
    cy1 = y + h - radius - 1;
    px = radius;
    py = 0;
    err = 1 - radius;
    while (px >= py) {
        put_pixel(cx0 - px, cy0 - py, color);
        put_pixel(cx0 - py, cy0 - px, color);
        put_pixel(cx1 + px, cy0 - py, color);
        put_pixel(cx1 + py, cy0 - px, color);
        put_pixel(cx0 - px, cy1 + py, color);
        put_pixel(cx0 - py, cy1 + px, color);
        put_pixel(cx1 + px, cy1 + py, color);
        put_pixel(cx1 + py, cy1 + px, color);
        py++;
        if (err < 0) err += 2 * py + 1;
        else {
            px--;
            err += 2 * (py - px) + 1;
        }
    }
    return 0;
}

/* ---------- 按钮 ---------- */
int ui_button_hit(const ui_button_t *btn, int x, int y)
{
    if (!btn) return 0;
    return (x >= btn->x && x < btn->x + btn->w &&
            y >= btn->y && y < btn->y + btn->h) ? 1 : 0;
}

int ui_draw_button(const ui_button_t *btn,
                   unsigned int fill, unsigned int border,
                   unsigned int text_fg, unsigned int text_bg, int text_scale)
{
    if (!btn) return -1;
    (void)text_bg;
    ui_fill_round_rect(btn->x, btn->y, btn->w, btn->h, 5, fill);
    ui_draw_round_rect(btn->x, btn->y, btn->w, btn->h, 5, border);
    if (btn->label && btn->label[0]) {
        int cx = btn->x + btn->w / 2;
        int cy = btn->y + btn->h / 2;
        ui_draw_text_center(cx, cy, btn->label,
                            text_fg, fill, text_scale);
    }
    return 0;
}

int ui_draw_button_pressed(const ui_button_t *btn)
{
    if (!btn) return -1;
    /* 按下态: 颜色变深 */
    unsigned int dark = UI_COLOR_DGRAY;
    ui_fill_round_rect(btn->x + 2, btn->y + 2,
                       btn->w - 4, btn->h - 4, 4, dark);
    if (btn->label && btn->label[0]) {
        int cx = btn->x + btn->w / 2;
        int cy = btn->y + btn->h / 2;
        ui_draw_text_center(cx, cy, btn->label,
                            UI_COLOR_WHITE, 0xFF000000u, 2);
    }
    return 0;
}
