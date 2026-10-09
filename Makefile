# ============================================================================
# RK3566 GUI 系统 Makefile
#  - 适用于 Linux + ARM64 (RK3566) 交叉编译
#  - 本地 PC (x86_64) 验证: 直接 `make` 会按本机架构编译
#  - 交叉编译示例:   make CROSS_COMPILE=aarch64-linux-gnu-
#                   或: make CC=aarch64-linux-gnu-gcc
#
#  目录结构:
#    gui/
#    ├── (本目录)             <-- 请在本目录下执行 make
#    │     ├── Makefile
#    │     ├── main.c
#    │     └── ...
#    └── reference/
#          ├── bmp_display.h   <-- 课程提供, 未随本仓库归档
#          └── bmp_display.c
# ============================================================================

# 工具链
CROSS_COMPILE ?=
CC            ?= $(CROSS_COMPILE)gcc
STRIP         ?= $(CROSS_COMPILE)strip

# 相对路径: 课程参考代码目录
REF_DIR = ./reference

# 编译选项
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu99 -D_GNU_SOURCE
CFLAGS  += -I. -I$(REF_DIR)
LDFLAGS ?=
LIBS    ?= -lpthread

# 目标
TARGET  = rk3566_gui

# 源文件
SRCS = main.c \
       input.c \
       ui.c \
       app_ui.c \
       animation.c \
       boot_anim.c \
       main_ui.c \
       album.c \
       login.c \
       paint.c \
       ball.c \
       snake.c \
       gomoku.c \
       game_menu.c \
       game_ui.c \
       touch_ctrl.c

# 参考代码 (BMP 解码与 framebuffer 基础接口, 直接链入)
REF_SRCS = $(REF_DIR)/bmp_display.c

OBJS = $(SRCS:.c=.o) $(REF_SRCS:.c=.o)

# 路径检查: 若参考代码目录不存在则立即报错
ifeq ($(wildcard $(REF_DIR)/bmp_display.h),)
$(error 找不到参考代码头文件: $(REF_DIR)/bmp_display.h  \
  该模块为课程提供的参考代码, 未随本仓库归档, 详见 reference/README.md)
endif

.PHONY: all clean run debug help

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)
	@echo "构建完成: $@"
	@ls -lh $@ 2>/dev/null || true

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 调试版本: 启用断言, 关闭优化
debug: CFLAGS = -O0 -g3 -Wall -Wextra -std=gnu99 -D_GNU_SOURCE -DDEBUG
debug: clean all

# 在开发主机上直接运行 (需要 framebuffer 设备)
run: $(TARGET)
	sudo ./$(TARGET)

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

# 静态检查 (可选)
check:
	@echo "源文件清单:"
	@ls -la $(SRCS) $(REF_SRCS) *.h $(REF_DIR)/*.h

# 帮助: 资源结构 + 编译运行说明
help:
	@echo ""
	@echo "============ 目录结构 ============"
	@echo "  gui/"
	@echo "  ├── src/                  源码 (请在本目录下执行 make)"
	@echo "  │     ├── Makefile"
	@echo "  │     ├── main.c ..."
	@echo "  └── reference/            课程参考代码 (BMP 解析+LCD 基础, 未归档)"
	@echo "        ├── bmp_display.h"
	@echo "        └── bmp_display.c"
	@echo ""
	@echo "============ 资源目录结构 (运行时, 可通过 RK_GUI_RES 覆盖) ============"
	@echo "  res/"
	@echo "  ├── boot/                开机动画帧 (BMP 24位)"
	@echo "  │     frame_000.bmp"
	@echo "  │     frame_001.bmp"
	@echo "  │     ..."
	@echo "  ├── shutdown/            关机动画帧 (BMP 24位)"
	@echo "  │     frame_000.bmp"
	@echo "  │     ..."
	@echo "  └── album/               相册图片 (BMP 24位, 任意命名)"
	@echo "        01.bmp, 02.bmp, ..."
	@echo ""
	@echo "============ 常用命令 ============"
	@echo "  make                    按本机架构编译"
	@echo "  make CROSS_COMPILE=aarch64-linux-gnu-  交叉编译 ARM64"
	@echo "  make run                在主机上以 root 权限运行 (需 framebuffer)"
	@echo "  make debug              调试版 (-O0 -g3 -DDEBUG)"
	@echo "  make clean              清理"
	@echo "  make help               显示本帮助"
	@echo ""
	@echo "============ 资源缺失时的降级行为 ============"
	@echo "  开机动画缺失 -> 显示文字 'RK3566 Booting...', 等待 2 秒后进入主界面"
	@echo "  关机动画缺失 -> 显示文字 'Shutting down...', 等待 1 秒后退出"
	@echo "  相册目录为空 -> 主界面相册入口仍可点击, 进入后显示空提示"
	@echo ""
	@echo "============ 功能模块 (v2.0) ============"
	@echo "  - Album   : 缩略图网格 / 单图 / 滑动切图 / 点击放大缩小"
	@echo "  - Paint   : 8 色调色板 / 3 档笔粗 / 撤销 / 清空"
	@echo "  - Login   : 软键盘输入 / 默认 admin / 123456 / 3 次失败可 Skip"
	@echo "  - Ball    : 板接球 / 挡板跟随手指 / 漏接结束"
	@echo "  - Snake   : 滑动控制方向 / 吃食物加速 / 撞墙/自撞结束"
	@echo "  - Gomoku  : 15x15 棋盘 / 双人轮流 / 5 连胜利 / 悔棋"
	@echo "  - 主界面  : 3 页 (多媒体/工具/游戏) 左右滑动切换"
	@echo ""
