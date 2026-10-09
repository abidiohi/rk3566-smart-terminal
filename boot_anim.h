/* ============================================================================
 * 文件: boot_anim.h
 * 说明: 开机/关机动画包装
 *      - 开机动画: 资源/boot/ 目录下 frame_000.bmp ... frame_NNN.bmp
 *      - 关机动画: 资源/shutdown/ 目录下 frame_000.bmp ... frame_NNN.bmp
 *      - 触摸屏幕任意位置可跳过开机动画
 *      - 关机动画播放完毕后函数不返回, 由调用方负责系统关机
 * ==========================================================================*/
#ifndef BOOT_ANIM_H
#define BOOT_ANIM_H

/* 默认资源根目录 (相对于程序运行目录) */
#define BOOT_ANIM_DEFAULT_ROOT   "./res"

/* 播放开机动画
 *  - root: 资源根目录, NULL 时使用 BOOT_ANIM_DEFAULT_ROOT
 *  - frame_ms: 单帧时长, 0 时使用默认 50ms
 *  - skip_on_touch: 1=触摸跳过, 0=不允许跳过(强制完整播放)
 *  返回: 0 正常播放完成, >0 用户跳过, -1 资源缺失
 */
int  boot_anim_play(const char *root, int frame_ms, int skip_on_touch);

/* 播放关机动画 (调用后建议直接执行系统关机命令) */
int  shutdown_anim_play(const char *root, int frame_ms);

#endif /* BOOT_ANIM_H */
