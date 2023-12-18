#ifndef __UI_IMAGE_H__
#define __UI_IMAGE_H__

#include "generic/typedef.h"

#define RGB565(r,g,b)   ((u16)(((u16)((u8)r)<<8)&0xF800) | (((u16)((u8)g)<<3) & 0x07E0) | ((u16)((u8)b)>>3))

enum {
    L1_TRANS_NONE,
    L1_TRANS_COLOR_INDEX,
    L1_TRANS_COLOR,
};
struct image_decode_var {
    u32 alpha_addr;
    u32 rle_offset;
    int temp_pixelbuf_len;
    int temp_alphabuf_len;
    u8 *pixelbuf;
    u8 *alphabuf;
    u8 *temp_pixelbuf;
    u8 *temp_alphabuf;
    RESFILE *fp;
    u8 quadrant;
    struct draw_context *dc;
    struct rect disp;//屏刷新区域
    struct rect r;//重叠区域
    int h;//重叠区域的h
    int hh;//缓存buffer的h
    int id;//控件id
    int page;//页面

    //input
    int remain;
    int vh; //旋转则为镜像后的h
    int vw; //旋转时则为镜像后的w

    //output
    struct rle_line *line;
    u8 *ptemp;
    u8 *p0;//指向ptemp

    struct rle_line *alpha_line;
    u8 *alpha_ptemp;
    u8 *p1;//指向alpha_ptemp

    u8 *lut;//颜色表
    u8 *unzip;

    int lines;//非压缩行数
};

struct image_priv {
    u8 transparent_mode;
    u8 transparent_color_index;
    u8 transparent_color_num;
    u32 transparent_color[16];
    void (*l1_lut_cb)(u16, u16, u16, u16 *);
    u16 prj;
    u16 page;
    u16 id;
    u16 lut[2];
};

void draw_image(struct image_file *file, struct image_decode_var *var);
void image_l1_transparent_color_set(u8 mode, u32 val);
void image_l1_set_lut_color_callback(void (*l1_lut_cb)(u16, u16, u16, u16 *));

#endif

