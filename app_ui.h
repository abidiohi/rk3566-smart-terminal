#ifndef APP_UI_H
#define APP_UI_H

#include "ui.h"

/* Shared visual language for system pages and game chrome. */
#define APP_COLOR_BG        0xFF0B111Au
#define APP_COLOR_HEADER    0xFF172231u
#define APP_COLOR_PANEL     0xFF1C293Au
#define APP_COLOR_SURFACE   0xFF223247u
#define APP_COLOR_SURFACE_2 0xFF293B52u
#define APP_COLOR_BORDER    0xFF3A5068u
#define APP_COLOR_PRIMARY   0xFF3DDC97u
#define APP_COLOR_MUTED     0xFF8FA6BCu
#define APP_COLOR_DANGER    0xFFE85D75u
#define APP_COLOR_AMBER     0xFFFFC857u
#define APP_COLOR_BLUE      0xFF52A8FFu
#define APP_COLOR_PURPLE    0xFFB879FFu

typedef struct {
    int x, y, w, h;
} app_rect_t;

typedef enum {
    APP_ICON_ALBUM = 0,
    APP_ICON_PAINT,
    APP_ICON_BOUNCE,
    APP_ICON_SNAKE,
    APP_ICON_GOMOKU,
    APP_ICON_SECURITY,
    APP_ICON_ABOUT,
    APP_ICON_POWER
} app_icon_t;

int  app_rect_hit(const app_rect_t *r, int x, int y);
void app_draw_surface(const app_rect_t *r, unsigned int fill,
                      unsigned int border, int radius);
void app_draw_icon(app_icon_t icon, int cx, int cy, int size,
                   unsigned int color);
void app_draw_chevron(int cx, int cy, int size, unsigned int color);

#endif
