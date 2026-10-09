/* ============================================================================
 * 文件: animation.h
 * 说明: 动画播放模块
 *      - 支持按目录编号顺序播放 BMP 帧序列 (frame_000.bmp ... frame_NNN.bmp)
 *      - 支持设置每帧停留时长
 *      - 支持循环 / 单次播放
 *      - 支持在任意时刻通过触摸事件中断
 * ==========================================================================*/
#ifndef ANIMATION_H
#define ANIMATION_H

/* 播放选项 */
typedef struct {
    const char *dir;          /* 帧目录路径 */
    const char *prefix;       /* 文件名前缀, 默认 "frame" */
    const char *suffix;       /* 文件名后缀, 默认 ".bmp" */
    int first_idx;            /* 起始编号, 默认 0 */
    int last_idx;             /* 结束编号 (含), -1 表示自动扫描 */
    int frame_ms;             /* 单帧时长(毫秒), 默认 50 */
    int loop;                 /* 0=播放一次结束, 1=循环 */
    int fill_bg;              /* 帧间是否用黑色填充背景, 1=是 */
    unsigned int bg_color;    /* 帧间背景色 */
} anim_opts_t;

/* 默认选项便捷宏 */
#define ANIM_OPTS_INIT(_dir) { \
    .dir = (_dir), .prefix = "frame", .suffix = ".bmp", \
    .first_idx = 0, .last_idx = -1, \
    .frame_ms = 50, .loop = 0, \
    .fill_bg = 1, .bg_color = 0xFF000000u }

/* 播放一帧序列
 *  - opts: 播放参数
 *  - interrupt: 若非 NULL, 在每帧之间检查, 非 0 时立即停止
 *  返回: 0 播放完成(非 loop), -1 文件缺失或参数错误, >0 用户中断时的中断值
 *
 *  备注: 该函数在播放期间不响应触摸(为开机动画设计), 
 *        如需可中断, 传入外部中断标记(例如全局原子标志).
 */
int  animation_play(const anim_opts_t *opts, volatile int *interrupt);

/* 扫描目录, 计算帧数 (写到 opts->last_idx, 若为 -1)
 *  返回: 实际帧数, -1 失败
 */
int  animation_scan(anim_opts_t *opts);

#endif /* ANIMATION_H */
