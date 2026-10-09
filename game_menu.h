/* ============================================================================
 * 文件: game_menu.h
 * 说明: 统一游戏菜单系统
 *      - 为贪吃蛇/弹球等游戏提供统一的菜单入口
 *      - 菜单风格与五子棋保持一致: 深灰顶栏/底栏, 彩色矩形按钮, 白色边框
 *      - 支持: 开始游戏 / 难度选择 / 排行榜 / 设置 / 退出
 *      - 交互: 点击直接选择, 上下滑动移动高亮, 点击确认
 * ==========================================================================*/
#ifndef GAME_MENU_H
#define GAME_MENU_H

/* 难度等级 */
typedef enum {
    GAME_DIFF_EASY   = 0,
    GAME_DIFF_MEDIUM = 1,
    GAME_DIFF_HARD   = 2
} game_difficulty_t;

/* 排行榜条目 */
#define GAME_LB_MAX   10

typedef struct {
    int score;
    /* 可扩展: char date[32]; char name[32]; */
} game_lb_entry_t;

/* 游戏设置项 (每个游戏可自定义) */
#define GAME_SETTINGS_MAX  5

typedef struct {
    const char *name;      /* 设置项名称 */
    int         value;     /* 当前值 */
    int         min_val;   /* 最小值 */
    int         max_val;   /* 最大值 */
    int         step;      /* 步进 */
} game_setting_t;

/* 菜单操作码 */
typedef enum {
    GMENU_NONE        = 0,
    GMENU_START_GAME  = 1,
    GMENU_CHANGE_DIFF = 2,
    GMENU_EXIT        = 3
} gmenu_result_t;

/* 游戏菜单上下文 */
typedef struct {
    const char       *title;              /* 游戏标题 (如 "Snake" / "Ball") */
    unsigned int      title_color;        /* 标题栏颜色 */
    game_difficulty_t difficulty;
    game_lb_entry_t   leaderboard[GAME_LB_MAX];
    int               lb_count;
    game_setting_t    settings[GAME_SETTINGS_MAX];
    int               settings_count;
} game_menu_ctx_t;

/* 运行游戏菜单
 *  ctx: 菜单上下文 (传入初始设置, 传出用户修改)
 *  返回: GMENU_START_GAME = 开始游戏, GMENU_EXIT = 退出菜单
 */
gmenu_result_t game_menu_run(game_menu_ctx_t *ctx);

/* 向排行榜插入一个分数 (自动排序, 保留前 GAME_LB_MAX 条) */
void game_menu_add_score(game_menu_ctx_t *ctx, int score);

/* 初始化菜单上下文 (需调用方先设置 title / title_color / settings) */
void game_menu_init(game_menu_ctx_t *ctx);

#endif /* GAME_MENU_H */
