# reference/ — 课程提供的参考代码（未归档）

本目录在原实训环境中存放课程方下发的 **BMP 图像解析 + LCD 基础接口** 参考模块：

```
参考代码/
├── bmp_display.h
└── bmp_display.c
```

该模块**不是团队成员编写**，也未随项目提交材料一同归档，因此本仓库中没有这两个文件。`Makefile` 在检测不到 `reference/bmp_display.h` 时会直接报错并提示，而不是产生一堆难以定位的编译错误。

## 接口清单

以下签名是从本仓库源码的**实际调用点**反推出来的，仅列出被使用的部分：

```c
/* 图像信息（以下字段在本仓库中被直接访问） */
typedef struct {
    unsigned int   width;      /* 图像宽（像素） */
    unsigned int   height;     /* 图像高（像素） */
    unsigned short bpp;        /* 位深；boot_anim.c 要求 24 */
    unsigned int   row_size;   /* 每行字节数（含 4 字节对齐填充） */
    /* ... 其余字段未在本仓库中使用 */
} bmp_info_t;

/* 读取 BMP 头部，成功返回 >= 0 */
int read_bmp_info(const char *path, bmp_info_t *info);

/* 读取像素数据，返回堆分配的缓冲区（24bpp BGR，行按 row_size 对齐），失败返回 NULL */
unsigned char *read_bmp_pixels(const char *path, bmp_info_t *info);

/* 最近邻缩放；dw/dh 传入原尺寸、传出缩放后尺寸；返回新的堆缓冲区或 NULL */
unsigned char *scale_bmp_nearest(const unsigned char *src,
                                 unsigned int src_w, unsigned int src_h,
                                 unsigned int src_row_size,
                                 double scale,
                                 unsigned int *dw, unsigned int *dh);
```

调用点：

| 文件 | 使用到的接口 |
|---|---|
| [`ui.c`](../ui.c) | `bmp_info_t`、`read_bmp_info`、`read_bmp_pixels`、`scale_bmp_nearest` |
| [`album.c`](../album.c) | `bmp_info_t`、`read_bmp_info`、`read_bmp_pixels`、`scale_bmp_nearest` |
| [`animation.c`](../animation.c) | `bmp_info_t`、`read_bmp_info` |
| [`boot_anim.c`](../boot_anim.c) | `bmp_info_t`、`read_bmp_info`、`read_bmp_pixels`、`scale_bmp_nearest` |
| [`ui.h`](../ui.h) | 头文件包含（复用类型定义） |

## 如果你想跑起来

按上面的接口清单补一个 `bmp_display.c`（或从课程资料中取回原文件）放到本目录，然后：

```bash
make CROSS_COMPILE=aarch64-linux-gnu-
```

本仓库对 `参考代码` 目录名的唯一依赖是 `Makefile` 中的 `REF_DIR`，可自由改写。
