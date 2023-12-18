/* COPYRIGHT NOTICE
 * 文件名称 ：ui_synthesis_oled.c
 * 简    介 ：OLED屏的合成
 * 功    能 ：
 * 作    者 ：zhuhaifang
 * 创建时间 ：2022/05/21 17:56
 */
#include "app_config.h"

#if (TCFG_LCD_OLED_ENABLE)

#include "ui/includes.h"
#include "ui/lcd_spi/lcd_drive.h"
#include "ui/ui_image.h"

#include "res/rle.h"

#define _RGB565(r,g,b)  (u16)((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3))
#define UI_RGB565(c)  \
        _RGB565((c>>16)&0xff,(c>>8)&0xff,c&0xff)

#define TEXT_MONO_CLR 0x555aaa
#define TEXT_MONO_INV 0xaaa555
#define RECT_MONO_CLR 0x555aaa
#define BGC_MONO_SET  0x555aaa


static struct ui_priv {
    struct lcd_info info;
};
static struct ui_priv priv ALIGNED(4);
#define __this (&priv)

struct fb_map_user {
    u16 xoffset;
    u16 yoffset;
    u16 width;
    u16 height;
    u8  *baddr;
    u8  *yaddr;
    u8  *uaddr;
    u8  *vaddr;
    u8 transp;
    u8 format;
};


static struct filepos {
    u32 offset;
    u32 len;
};

void set_screen_info(struct lcd_info *info)
{
    memcpy(&__this->info, info, sizeof(struct lcd_info));
}


AT_UI_RAM
static int line_update(u8 *mask, u16 y, u16 width)
{
    int i;
    if (!mask) {
        return true;
    }
    for (i = 0; i < (width + 7) / 8; i++) {
        if (mask[y * ((width + 7) / 8) + i]) {
            return true;
        }
    }
    return false;
}

static u16 crc_16(u16 crc_val, u8 dat)
{
    static const u16 crc_ta[16] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    };
    u8 da;

    da = crc_val >> 12;
    crc_val <<= 4;
    crc_val ^= crc_ta[da ^ (dat / 16)];

    da = crc_val >> 12;
    crc_val <<= 4;
    crc_val ^= crc_ta[da ^ (dat & 0x0f)];

    return crc_val;
}


static void l1_lut_callback(u16 prj, u16 page, u16 id, u16 *lut)
{
    printf("[5]lut : %d-%d-%d, 0x%04x, 0x%04x\n", prj, page, id, lut[0], lut[1]);
    lut[1] = RGB565(0xff, 0, 0);
}

/* 透明色: 16bits 0x55aa      0101 0xxx 1011 01xx 0101 0xxx
 *         24bits 0x50b450    0101 0000 1011 0100 0101 0000 , 80 180 80
 * */
void __font_pix_copy(struct draw_context *dc, int format, struct fb_map_user *map, u8 *pix, struct rect *draw, int x, int y,
                     int height, int width, int color)
{

    int i, j, h;
    u16 osd_color;
    u32 size;

    osd_color = (format == DC_DATA_FORMAT_OSD8) || (format == DC_DATA_FORMAT_OSD8A) ? color & 0xff : color & 0xffff ;

    for (j = 0; j < (height + 7) / 8; j++) { /* 纵向8像素为1字节 */
        for (i = 0; i < width; i++) {
            if (((i + x) >= draw->left)
                && ((i + x) <= (draw->left + draw->width - 1))) { /* x在绘制区域，要绘制 */
                u8 pixel = pix[j * width + i];
                int hh = height - (j * 8);
                if (hh > 8) {
                    hh = 8;
                }
                for (h = 0; h < hh; h++) {
                    if (((y + j * 8 + h) >= draw->top)
                        && ((y + j * 8 + h) <= (draw->top + draw->height - 1))) { /* y在绘制区域，要绘制 */
                        u16 clr = pixel & BIT(h) ? osd_color : 0;
                        if (clr) {
                            if (platform_api->draw_point) {
                                platform_api->draw_point(dc, x + i, y + j * 8 + h, clr);
                            }
                        }
                    }
                } /* endof for h */
            }
        }/* endof for i */
    }/* endof for j */
}


static int image_str_size_check(int page_num, const char *txt, int *width, int *height)
{

    u16 id = ((u8)txt[1] << 8) | (u8)txt[0];
    u16 cnt = 0;
    struct image_file file;
    int w = 0, h = 0;

    while (id != 0x00ff) {
        if (open_image_by_id(NULL, &file, id, page_num) != 0) {
            return -EFAULT;
        }
        w += file.width;
        cnt += 2;
        id = ((u8)txt[cnt + 1] << 8) | (u8)txt[cnt];
    }
    h = file.height;
    *width = w;
    *height = h;
    return 0;
}

void platform_putchar(struct font_info *info, u8 *pixel, u16 width, u16 height, u16 x, u16 y)
{
    __font_pix_copy(info->dc, info->disp.format,
                    (struct fb_map_user *)info->disp.map,
                    pixel,
                    (struct rect *)info->disp.rect,
                    x,
                    y,
                    height,
                    width,
                    info->disp.color);
}




int jlui_invert_rect(struct draw_context *dc, u32 acolor)
{
    int i;
    int len;
    int w, h;
    int color = acolor & 0xffff;

    if (dc->data_format == DC_DATA_FORMAT_MONO) {
        color |= BIT(31);
        for (h = 0; h < dc->draw.height; h++) {
            for (w = 0; w < dc->draw.width; w++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, dc->draw.left + w, dc->draw.top + h, color);
                }
            }
        }
    }
    return 0;
}

int jlui_fill_rect(struct draw_context *dc, u32 acolor)
{
    int i;
    int w, h;
    u16 color = acolor & 0xffff;

    if (!dc->buf) {
        return 0;
    }

    if (dc->data_format == DC_DATA_FORMAT_MONO) {
        color = (color == UI_RGB565(BGC_MONO_SET)) ? 0xffff : 0x55aa;

        for (h = 0; h < dc->draw.height; h++) {
            for (w = 0; w < dc->draw.width; w++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, dc->draw.left + w, dc->draw.top + h, color);
                }
            }
        }
    } else {
        u16 color16 = (color >> 8) | ((color & 0xff) << 8);
        u32 color32 = (color16 << 16) | color16;

        h = 0;
        u32 *p32 = (u32 *)&dc->buf[(dc->draw.top + h - dc->disp.top) * dc->disp.width * 2 + (dc->draw.left - dc->disp.left) * 2];
        u32 *_p32 = p32;
        u32 len = dc->draw.width * 2;
        if ((u32)p32 % 4) {
            u16 *p16 = (u16 *)p32;
            *p16++ = color16;
            p32 = (u32 *)p16;
            len -= 2;
            ASSERT((u32)p32 % 4 == 0);
        }

        u32 count = len / 4;
        while (count--) {
            *p32++ = color32;
        }
        count = (len % 4) / 2;
        u16 *p16 = (u16 *)p32;
        while (count--) {
            *p16++ = color16;
        }

        for (h = 1; h < dc->draw.height; h++) {
            u32 *__p32 = (u32 *)&dc->buf[(dc->draw.top + h - dc->disp.top) * dc->disp.width * 2 + (dc->draw.left - dc->disp.left) * 2];
            memcpy(__p32, _p32, dc->draw.width * 2);
        }
    }

    return 0;
}

static inline void __draw_vertical_line(struct draw_context *dc, int x, int y, int width, int height, int color, int format)
{
    int i, j;
    struct rect r = {0};
    struct rect disp = {0};

    disp.left  = x;
    disp.top   = y;
    disp.width = width;
    disp.height = height;
    if (!get_rect_cover(&dc->draw, &disp, &r)) {
        return;
    }

    switch (format) {
    case DC_DATA_FORMAT_OSD16:
        for (i = 0; i < r.width; i++) {
            for (j = 0; j < r.height; j++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, r.left + i, r.top + j, color);
                }
            }
        }
        break;
    case DC_DATA_FORMAT_MONO:
        for (i = 0; i < r.width; i++) {
            for (j = 0; j < r.height; j++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, r.left + i, r.top + j, color);
                }
            }
        }
        break;

    }
}

static inline void __draw_line(struct draw_context *dc, int x, int y, int width, int height, int color, int format)
{
    int i, j;
    struct rect r = {0};
    struct rect disp = {0};

    disp.left  = x;
    disp.top   = y;
    disp.width = width;
    disp.height = height;
    if (!get_rect_cover(&dc->draw, &disp, &r)) {
        return;
    }

    switch (format) {
    case DC_DATA_FORMAT_OSD16:
        for (i = 0; i < r.height; i++) {
            for (j = 0; j < r.width; j++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, r.left + j, r.top + i, color);
                }
            }
        }
        break;
    case DC_DATA_FORMAT_MONO:
        for (i = 0; i < r.height; i++) {
            for (j = 0; j < r.width; j++) {
                if (platform_api->draw_point) {
                    platform_api->draw_point(dc, r.left + j, r.top + i, color);
                }
            }
        }
        break;
    }
}

int jlui_draw_rect(struct draw_context *dc, struct css_border *border)
{
    int err;
    int offset;
    int color = border->color & 0xffff;

    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        color = border->color & 0xffff;
    } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
        color = (color != UI_RGB565(RECT_MONO_CLR)) ? (color ? color : 0xffff) : 0x55aa;
    }

    if (border->left) {
        if (dc->rect.left >= dc->draw.left &&
            dc->rect.left <= rect_right(&dc->draw)) {
            __draw_vertical_line(dc, dc->draw.left, dc->draw.top,
                                 border->left, dc->draw.height, color, dc->data_format);
        }
    }
    if (border->right) {
        if (rect_right(&dc->rect) >= dc->draw.left &&
            rect_right(&dc->rect) <= rect_right(&dc->draw)) {
            __draw_vertical_line(dc, dc->draw.left + dc->draw.width - border->right, dc->draw.top,
                                 border->right, dc->draw.height, color, dc->data_format);
        }
    }
    if (border->top) {
        if (dc->rect.top >= dc->draw.top &&
            dc->rect.top <= rect_bottom(&dc->draw)) {
            __draw_line(dc, dc->draw.left, dc->draw.top,
                        dc->draw.width, border->top, color, dc->data_format);
        }
    }
    if (border->bottom) {
        if (rect_bottom(&dc->rect) >= dc->draw.top &&
            rect_bottom(&dc->rect) <= rect_bottom(&dc->draw)) {
            __draw_line(dc, dc->draw.left, dc->draw.top + dc->draw.height - border->bottom,
                        dc->draw.width, border->bottom, color, dc->data_format);
        }
    }

    return 0;
}

__attribute__((always_inline_when_const_args))
AT_UI_RAM
static u16 get_mixed_pixel(u16 backcolor, u16 color, u8 alpha)
{
    u16 mixed_color;
    u8 r0, g0, b0;
    u8 r1, g1, b1;
    u8 r2, g2, b2;

    if (alpha == 0) {
        return backcolor;
    }

    r0 = ((backcolor >> 11) & 0x1f) << 3;
    g0 = ((backcolor >> 5) & 0x3f) << 2;
    b0 = ((backcolor >> 0) & 0x1f) << 3;

    r1 = ((color >> 11) & 0x1f) << 3;
    g1 = ((color >> 5) & 0x3f) << 2;
    b1 = ((color >> 0) & 0x1f) << 3;

    r2 = (alpha * r1 + (255 - alpha) * r0) / 255;
    g2 = (alpha * g1 + (255 - alpha) * g0) / 255;
    b2 = (alpha * b1 + (255 - alpha) * b0) / 255;

    mixed_color = ((r2 >> 3) << 11) | ((g2 >> 2) << 5) | (b2 >> 3);

    return (mixed_color >> 8) | (mixed_color & 0xff) << 8;
}


AT_UI_RAM
int jlui_draw_image(struct draw_context *dc, u32 src, u8 quadrant, u8 *mask)
{
    u8 *pixelbuf;
    u8 *temp_pixelbuf;
    u8 *alphabuf;
    u8 *temp_alphabuf;
    struct rect draw_r;
    struct rect r = {0};
    struct rect disp = {0};
    struct image_file file;
    int h, hh, w;
    int buf_offset;
    int id;
    int page;
    FILE *fp;

    if (dc->preview.file) {
        fp = dc->preview.file;
        id = dc->preview.id;
        page = dc->preview.page;
    } else {
        fp = NULL;
        id = src;
        page = dc->page;
    }

    if (((u16) - 1 == id) || ((u32) - 1 == id)) {
        return -1;
    }

    draw_r.left   = dc->draw.left;
    draw_r.top    = dc->draw.top;
    draw_r.width  = dc->draw.width;
    draw_r.height = dc->draw.height;


    int err = open_image_by_id(fp, &file, id, page);
    if (err) {
        return -EFAULT;
    }

    int x = dc->rect.left;
    int y = dc->rect.top;

    if (dc->align == UI_ALIGN_CENTER) {
        x += (dc->rect.width / 2 - file.width / 2);
        y += (dc->rect.height / 2 - file.height / 2);
    } else if (dc->align == UI_ALIGN_RIGHT) {
        x += dc->rect.width - file.width;
    }

    int temp_pixelbuf_len = dc->width * 2 * dc->lines + 0x40 - 8;
    int temp_alphabuf_len = dc->width * dc->lines + 0x40 - 8;
    int align;

    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        buf_offset = 0;
        pixelbuf = &dc->fbuf[buf_offset];//2 bytes * line
        buf_offset += dc->width * 2;
        buf_offset = (buf_offset + 3) / 4 * 4;
        alphabuf = &dc->fbuf[buf_offset];//1 bytes * line
        buf_offset += dc->width;
        buf_offset = (buf_offset + 3) / 4 * 4;
        temp_pixelbuf = &dc->fbuf[buf_offset];
        buf_offset += temp_pixelbuf_len;
        buf_offset = (buf_offset + 3) / 4 * 4;
        temp_alphabuf = &dc->fbuf[buf_offset];
    } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
        pixelbuf = dc->fbuf;
    } else {
        ASSERT(0);
    }

    disp.left   = x;
    disp.top    = y;
    disp.width  = file.width;
    disp.height = file.height;

    if (dc->data_format == DC_DATA_FORMAT_MONO) {
        if (get_rect_cover(&draw_r, &disp, &r)) {
            int _offset = -1;
            for (h = 0; h < r.height; h++) {
                if (file.compress == 0) {
                    int offset = (r.top + h - disp.top) / 8 * file.width + (r.left - disp.left);
                    if (_offset != offset) {
                        if (br23_read_image_data(fp, &file, pixelbuf, r.width, offset) != r.width) {
                            return -EFAULT;
                        }
                        _offset = offset;
                    }
                } else {
                    ASSERT(0, "the compress mode not support!");
                }

                for (w = 0; w < r.width; w++) {
                    u8 color = (pixelbuf[w] & BIT((r.top + h - disp.top) % 8)) ? 1 : 0;
                    if (color) {
                        if (platform_api->draw_point) {
                            platform_api->draw_point(dc, r.left + w, r.top + h, color);
                        }
                    }
                }
            }
        }
    } else if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        if (get_rect_cover(&draw_r, &disp, &r)) {
            u32 alpha_addr = 0;
            br23_read_image_data(fp, &file, &alpha_addr, sizeof(alpha_addr), 0);

            for (h = 0; h < r.height;) {
                int rh = r.top + h - disp.top;
                int rw = r.left - disp.left;
                int vh = rh;
                int vw = rw;
                if (quadrant == 0) {
                    ;
                } else if (quadrant == 1) {
                    vh = file.height - rh - 1;
                } else if (quadrant == 2) {
                    vh = file.height - rh - 1;
                    vw = file.width - rw - r.width;
                } else {
                    vw = file.width - rw - r.width;
                }
                ASSERT(vw >= 0);
                ASSERT(vh >= 0);

                struct rle_line *line;
                struct rle_line *alpha_line;
                u8 *ptemp;
                u8 *alpha_ptemp;
                int lines;

                if (file.compress == 0) {
                    int remain = (r.height - h) > (file.height - vh) ? (file.height - vh) : (r.height - h);
                    int offset = 4 + vh * file.width * 2 + vw * 2;

                    br23_read_image_data(fp, &file, &alpha_addr, sizeof(alpha_addr), 0);
                    if (!alpha_addr) {
                        lines = dc->fbuf_len / file.width / 2;
                    } else {
                        lines = dc->fbuf_len / file.width / 3;
                    }
                    lines = (lines > remain) ? remain : lines;
                    if ((quadrant == 1) || (quadrant == 2)) {
                        lines = 1;
                    }
                    pixelbuf = dc->fbuf;
                    alphabuf = &dc->fbuf[(lines * file.width * 2 + 3) / 4 * 4];

                    if (br23_read_image_data(fp, &file, pixelbuf, file.width * 2 * lines, offset) != file.width * 2 * lines) {
                        return -EFAULT;
                    }
                    if (alpha_addr) {
                        offset = alpha_addr + vh * file.width + vw;
                        if (br23_read_image_data(fp, &file, alphabuf, file.width * lines, offset) != file.width * lines) {
                            return -EFAULT;
                        }
                    }
                } else if (file.compress == 1) {
                    int remain = (r.height - h) > (file.height - vh) ? (file.height - vh) : (r.height - h);
                    int headlen = sizeof(struct rle_header) + (remain * 2 + 3) / 4 * 4;

                    line = (struct rle_line *)temp_pixelbuf;
                    ptemp = &temp_pixelbuf[headlen];
                    memset(line, 0x00, sizeof(struct rle_line));

                    br23_read_image_data(fp, &file, ptemp, sizeof(struct rle_header)*remain, 4 + vh * sizeof(struct rle_header));

                    int i;
                    struct rle_header *rle = (struct rle_header *)ptemp;
                    int total_len = 0;
                    for (i = 0; i < remain; i++) {
                        if (i == 0) {
                            line->addr = rle[i].addr;
                            line->len[i] = rle[i].len;
                        } else {
                            line->len[i] = rle[i].len;
                        }
                        if ((total_len + rle[i].len) > (temp_pixelbuf_len - headlen)) {
                            break;
                        }
                        total_len += rle[i].len;
                        line->num ++;
                        if ((quadrant == 1) || (quadrant == 2)) {
                            break;
                        }
                    }
                    br23_read_image_data(fp, &file, ptemp, total_len, 4 + line->addr);

                    if (alpha_addr) {
                        int headlen = sizeof(struct rle_header) + (line->num * 2 + 3) / 4 * 4;
                        alpha_ptemp = &temp_alphabuf[headlen];
                        br23_read_image_data(fp, &file, alpha_ptemp, sizeof(struct rle_header)*line->num, alpha_addr + vh * sizeof(struct rle_header));

                        struct rle_header *rle = (struct rle_header *)alpha_ptemp;
                        alpha_line = (struct rle_line *)temp_alphabuf;
                        memset(alpha_line, 0x00, sizeof(struct rle_line));
                        int total_len = 0;
                        for (i = 0; i < line->num; i++) {
                            if (i == 0) {
                                alpha_line->addr = rle[i].addr;
                                alpha_line->len[i] = rle[i].len;
                            } else {
                                alpha_line->len[i] = rle[i].len;
                            }
                            if ((total_len + rle[i].len) > (temp_alphabuf_len - headlen)) {
                                break;
                            }
                            total_len += rle[i].len;
                            alpha_line->num ++;
                        }

                        br23_read_image_data(fp, &file, alpha_ptemp, total_len, alpha_addr + alpha_line->addr);
                    }
                } else {
                    ASSERT(0, "the compress mode not support!");
                }

                u8 *p0 = ptemp;
                u8 *p1 = alpha_ptemp;
                int line_num;
                if (file.compress == 0) {
                    /* line_num = 1; */
                    line_num = lines;
                } else {
                    if (alpha_addr) {
                        line_num = (line->num > alpha_line->num) ? alpha_line->num : line->num;
                    } else {
                        line_num = line->num;
                    }
                }

                for (hh = 0; hh < line_num; hh++, h++) {
                    if (file.compress == 1) {
                        if (line_update(mask, r.top + h - dc->disp.top, dc->disp.width)) {
                            Rle_Decode(p0, line->len[hh], pixelbuf, file.width * 2, vw * 2, r.width * 2, 2);
                            p0 += line->len[hh];
                            if (alpha_addr) {
                                Rle_Decode(p1, alpha_line->len[hh], alphabuf, file.width, vw, r.width, 1);
                                p1 += alpha_line->len[hh];
                            }
                        } else {
                            p0 += line->len[hh];
                            p1 += alpha_line->len[hh];
                            continue;
                        }
                    }

                    u16 *pdisp = (u16 *)dc->buf;
                    u16 *pixelbuf16 = (u16 *)pixelbuf;

                    if (!alpha_addr) {
                        u16 x0 = r.left + 0/* vww */;
                        u16 y0 = r.top + h;
                        int offset = (y0 - dc->disp.top) * dc->disp.width + (x0 - dc->disp.left);
                        /* if ((offset * 2 + 1) < dc->len) { */
                        /* pdisp[offset] = pixel; */
                        /* } */
                        memcpy(&pdisp[offset], pixelbuf16, r.width * 2);
                        if (file.compress == 0) {
                            pixelbuf += file.width * 2;
                        }
                        continue;
                    }

                    for (w = 0; w < r.width; w++) {
                        u16 color, pixel;
                        u8  alpha = alpha_addr ? alphabuf[w] : 255;

                        pixel = color = pixelbuf16[w];
                        if (color) {
                            if (platform_api->draw_point) {
                                int vww = w;
                                if ((quadrant == 2) || (quadrant == 3)) {
                                    vww = r.width - w - 1;
                                }
                                u16 x0 = r.left + vww;
                                u16 y0 = r.top + h;

                                if (alpha < 255) {
                                    u16 backcolor = platform_api->read_point(dc, x0, y0);
                                    pixel = get_mixed_pixel(backcolor >> 8 | (backcolor & 0xff) << 8, color>>8 | (color & 0xff) << 8, alpha);
                                }

                                if (mask) {
                                    int yy = y0 - dc->disp.top;
                                    int xx = x0 - dc->disp.left;
                                    if (yy >= dc->disp.height) {
                                        continue;
                                    }
                                    if (xx >= dc->disp.width) {
                                        continue;
                                    }
                                    if (mask[yy * ((dc->disp.width + 7) / 8) + xx / 8] & BIT(xx % 8)) {
                                        int offset = (y0 - dc->disp.top) * dc->disp.width + (x0 - dc->disp.left);
                                        if ((offset * 2 + 1) < dc->len) {
                                            pdisp[offset] = pixel;
                                        }
                                    }
                                } else {
                                    int offset = (y0 - dc->disp.top) * dc->disp.width + (x0 - dc->disp.left);
                                    if ((offset * 2 + 1) < dc->len) {
                                        pdisp[offset] = pixel;
                                    }
                                }
                            }
                        }
                    }

                    if (file.compress == 0) {
                        pixelbuf += file.width * 2;
                        alphabuf += file.width;
                    }
                }
            }
        }
    }

    return 0;
}

int get_multi_string_width(u8 *str, int *str_width, int *str_height)
{
    struct image_file file;
    u8 *p = str;
    int width = 0;
    while (*p != 0) {
        if (open_string_pic(&file, *p)) {
            return -EINVAL;
        }
        width += file.width;
        p++;
    }
    *str_width = width;
    *str_height = file.height;

    return 0;
}


int jlui_show_text(struct draw_context *dc, struct ui_text_attrs *text)
{
    struct rect draw_r;
    struct rect r = {0};
    struct rect disp = {0};
    struct image_file file;


    /* 控件从绝对x,y 转成相对图层的x,y */
    int x = dc->rect.left;
    int y = dc->rect.top;

    /* 绘制区域从绝对x,y 转成相对图层的x,y */
    draw_r.left   = dc->draw.left;
    draw_r.top    = dc->draw.top;
    draw_r.width  = dc->draw.width;
    draw_r.height = dc->draw.height;

    if (text->format && !strcmp(text->format, "text")) {
        static struct font_info *info = NULL;
        static int language = 0;
        if (!info || (language != ui_language_get())) {
            language = ui_language_get();
            if (info) {
                font_close(info);
            }
            info = font_open(NULL, language);
            ASSERT(info, "font_open fail!");
        }
        if (info && (FT_ERROR_NONE == (info->sta & (~FT_ERROR_NOTABFILE | FT_ERROR_NOPIXFILE)))) {
            info->disp.map    = 0;
            info->disp.rect   = &draw_r;
            info->disp.format = dc->data_format;
            if ((dc->data_format == DC_DATA_FORMAT_MONO) && (text->color == UI_RGB565(TEXT_MONO_INV))) {
                /* if (__this->api->fill_rect) { */
                /* __this->api->fill_rect(dc, UI_RGB565(BGC_MONO_SET)); */
                /* } */
                jlui_fill_rect(dc, UI_RGB565(BGC_MONO_SET));
            }
            if (dc->data_format == DC_DATA_FORMAT_OSD16) {
                info->disp.color  = text->color;
            } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
                if (text->color == UI_RGB565(TEXT_MONO_INV)) {
                    info->disp.color = 0x55aa;//清显示
                } else {
                    info->disp.color = (text->color != UI_RGB565(TEXT_MONO_CLR)) ? (text->color ? text->color : 0xffff) : 0x55aa;
                }
            }
            info->dc = dc;

            info->text_width  = draw_r.width;
            info->text_height = draw_r.height;
            info->flags       = text->flags;
            /* info->offset      = text->offset; */
            int roll = 0;//需要滚动
            int multi_line = 0;
            /* FONT_SHOW_MULTI_LINE */
            if (text->encode == FONT_ENCODE_ANSI) {
                int width = font_text_width(info, (u8 *)text->str, text->strlen);
                int height;


                if (info->ascpixel.size) {
                    height = info->ascpixel.size;
                } else if (info->pixel.size) {
                    height = info->pixel.size;
                } else {
                    ASSERT(0, "can't get the height of font.");
                }

                if (width > dc->rect.width) {
                    width = dc->rect.width;
                    roll = 1;
                    multi_line = 1;
                }


                if (text->flags & FONT_SHOW_MULTI_LINE) {
                    height += multi_line * height;
                }

                if (height > dc->rect.height) {
                    height = dc->rect.height;
                }

                y += (dc->rect.height / 2 - height / 2);

                if (dc->align == UI_ALIGN_CENTER) {
                    x += (dc->rect.width / 2 - width / 2);
                } else if (dc->align == UI_ALIGN_RIGHT) {
                    x += (dc->rect.width - width);
                }
                info->x = x;
                info->y = y;
                int len = font_textout(info, (u8 *)(text->str + roll * text->offset * 2), text->strlen - roll * text->offset * 2, x, y);
                ASSERT(len <= 255);
                text->displen = len;
            } else if (text->encode == FONT_ENCODE_UNICODE) {
                if (FT_ERROR_NONE == (info->sta & FT_ERROR_NOTABFILE)) {
                    if (text->endian == FONT_ENDIAN_BIG) {
                        info->bigendian = true;
                    } else {
                        info->bigendian = false;
                    }
                    int width = font_textw_width(info, (u8 *)text->str, text->strlen);
                    int height;

                    if (info->ascpixel.size) {
                        height = info->ascpixel.size;
                    } else if (info->pixel.size) {
                        height = info->pixel.size;
                    } else {
                        ASSERT(0, "can't get the height of font.");
                    }

                    if (width > dc->rect.width) {
                        width = dc->rect.width;
                        roll = 1;
                        multi_line = 1;
                    }

                    if (text->flags & FONT_SHOW_MULTI_LINE) {
                        height += multi_line * height;
                    }


                    if (height > dc->rect.height) {
                        height = dc->rect.height;
                    }

                    y += (dc->rect.height / 2 - height / 2);
                    if (dc->align == UI_ALIGN_CENTER) {
                        x += (dc->rect.width / 2 - width / 2);
                    } else if (dc->align == UI_ALIGN_RIGHT) {
                        x += (dc->rect.width - width);
                    }

                    info->x = x;
                    info->y = y;
                    int len = font_textout_unicode(info, (u8 *)(text->str + roll * text->offset * 2), text->strlen - roll * text->offset * 2, x, y);
                    ASSERT(len <= 255);
                    text->displen = len;
                }
            } else {
                int width = font_textu_width(info, (u8 *)text->str, text->strlen);
                int height;

                if (info->ascpixel.size) {
                    height = info->ascpixel.size;
                } else if (info->pixel.size) {
                    height = info->pixel.size;
                } else {
                    ASSERT(0, "can't get the height of font.");
                }

                if (width > dc->rect.width) {
                    width = dc->rect.width;
                }
                if (height > dc->rect.height) {
                    height = dc->rect.height;
                }

                y += (dc->rect.height / 2 - height / 2);
                if (dc->align == UI_ALIGN_CENTER) {
                    x += (dc->rect.width / 2 - width / 2);
                } else if (dc->align == UI_ALIGN_RIGHT) {
                    x += (dc->rect.width - width);
                }

                info->x = x;
                info->y = y;
                int len = font_textout_utf8(info, (u8 *)text->str, text->strlen, x, y);
                ASSERT(len <= 255);
                text->displen = len;
            }
        }
    } else if (text->format && !strcmp(text->format, "ascii")) {
        char *str;
        u32 w_sum;
        if (!text->str) {
            return 0;
        }
        if ((u8)text->str[0] == 0xff) {
            return 0;
        }

        if (dc->align == UI_ALIGN_CENTER) {
            w_sum = font_ascii_width_check(text->str);
            x += (dc->rect.width / 2 - w_sum / 2);
        } else if (dc->align == UI_ALIGN_RIGHT) {
            w_sum = font_ascii_width_check(text->str);
            x += (dc->rect.width - w_sum);
        }

        if ((dc->data_format == DC_DATA_FORMAT_MONO) && (text->color == UI_RGB565(TEXT_MONO_INV))) {
            /* if (__this->api->fill_rect) { */
            /* __this->api->fill_rect(dc, UI_RGB565(BGC_MONO_SET)); */
            /* } */
            jlui_fill_rect(dc, UI_RGB565(BGC_MONO_SET));
        }
        str = text->str;
        while (*str) {
            u8 *pixbuf = dc->fbuf;
            int width;
            int height;
            int color;
            font_ascii_get_pix(*str, pixbuf, dc->fbuf_len, &height, &width);
            if (dc->data_format == DC_DATA_FORMAT_OSD16) {
                color  = text->color;
            } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
                if (text->color == UI_RGB565(TEXT_MONO_INV)) {
                    color = 0x55aa;//清显示
                } else {
                    color = (text->color != UI_RGB565(TEXT_MONO_CLR)) ? (text->color ? text->color : 0xffff) : 0x55aa;
                }
            }
            __font_pix_copy(dc, dc->data_format, 0, pixbuf, &draw_r, x, y, height, width, color);
            x += width;
            str++;
        }
    } else if (text->format && !strcmp(text->format, "strpic")) {
        u16 id = (u8)text->str[0];
        u8 *pixbuf;
        int w;
        int h;

        if (id == 0xffff) {
            return 0;
        }

        if (open_string_pic(&file, id)) {
            return 0;
        }

        y += (dc->rect.height / 2 - file.height / 2);
        if (dc->align == UI_ALIGN_CENTER) {
            x += (dc->rect.width / 2 - file.width / 2);
        } else if (dc->align == UI_ALIGN_RIGHT) {
            x += (dc->rect.width - file.width);
        }

        pixbuf = dc->fbuf;
        if (!pixbuf) {
            return -ENOMEM;
        }

        disp.left   = x;
        disp.top    = y;
        disp.width  = file.width;
        disp.height = file.height;

        if (get_rect_cover(&draw_r, &disp, &r)) {
            if ((dc->data_format == DC_DATA_FORMAT_MONO) && (text->color == UI_RGB565(TEXT_MONO_INV))) {
                /* if (__this->api->fill_rect) { */
                /* __this->api->fill_rect(dc, UI_RGB565(BGC_MONO_SET)); */
                /* } */
                jlui_fill_rect(dc, UI_RGB565(BGC_MONO_SET));
            }
            for (h = 0; h < file.height; h += 8) {
                if (file.compress == 0) {
                    int offset = (h / 8) * file.width;
                    if (br23_read_str_data(&file, pixbuf, file.width, offset) != file.width) {
                        return -EFAULT;
                    }
                } else {
                    ASSERT(0, "the compress mode not support!");
                }
                int color;
                if (dc->data_format == DC_DATA_FORMAT_OSD16) {
                    color  = text->color;
                } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
                    if (text->color == UI_RGB565(TEXT_MONO_INV)) {
                        color = 0x55aa;//清显示
                    } else {
                        color = (text->color != UI_RGB565(TEXT_MONO_CLR)) ? (text->color ? text->color : 0xffff) : 0x55aa;
                    }
                }
                __font_pix_copy(dc, dc->data_format, 0, pixbuf, &r, x, y + h / 8 * 8, 8, file.width, color);
            }
        }
    } else if (text->format && !strcmp(text->format, "mulstr")) {
        u16 id = (u8)text->str[0];
        u8 *pixbuf;
        int w;
        int h;
        u8 *p = text->str;

        if (get_multi_string_width(text->str, &w, &h)) {
            return -EINVAL;
        }

        y += (dc->rect.height / 2 - h / 2);
        if (dc->align == UI_ALIGN_CENTER) {
            x += (dc->rect.width / 2 - w / 2);
        } else if (dc->align == UI_ALIGN_RIGHT) {
            x += (dc->rect.width - w);
        }

        while (*p != 0) {
            id = *p;

            if (id == 0xffff) {
                return 0;
            }

            if (open_string_pic(&file, id)) {
                return 0;
            }


            pixbuf = dc->fbuf;
            if (!pixbuf) {
                return -ENOMEM;
            }

            disp.left   = x;
            disp.top    = y;
            disp.width  = file.width;
            disp.height = file.height;

            if (get_rect_cover(&draw_r, &disp, &r)) {
                if ((dc->data_format == DC_DATA_FORMAT_MONO) && (text->color == UI_RGB565(TEXT_MONO_INV))) {
                    /* if (__this->api->fill_rect) { */
                    /* __this->api->fill_rect(dc, UI_RGB565(BGC_MONO_SET)); */
                    /* } */
                    jlui_fill_rect(dc, UI_RGB565(BGC_MONO_SET));
                }
                for (h = 0; h < file.height; h += 8) {
                    if (file.compress == 0) {
                        int offset = (h / 8) * file.width;
                        if (br23_read_str_data(&file, pixbuf, file.width, offset) != file.width) {
                            return -EFAULT;
                        }
                    } else {
                        ASSERT(0, "the compress mode not support!");
                    }
                    int color;
                    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
                        color  = text->color;
                    } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
                        if (text->color == UI_RGB565(TEXT_MONO_INV)) {
                            color = 0x55aa;//清显示
                        } else {
                            color = (text->color != UI_RGB565(TEXT_MONO_CLR)) ? (text->color ? text->color : 0xffff) : 0x55aa;
                        }
                    }
                    __font_pix_copy(dc, dc->data_format, 0, pixbuf, &r, x, y + h / 8 * 8, 8, file.width, color);
                }
            }
            x += file.width;
            p++;
        }

    } else if (text->format && !strcmp(text->format, "image")) {
        u8 *pixelbuf;
        u8 *temp_pixelbuf;
        u8 *alphabuf;
        u8 *temp_alphabuf;
        u16 cnt = 0;
        u16 id = ((u8)text->str[1] << 8) | (u8)text->str[0];
        u32 w, h;
        int ww, hh;


        if (image_str_size_check(dc->page, text->str, &ww, &hh) != 0) {
            return -EFAULT;
        }
        if (dc->align == UI_ALIGN_CENTER) {
            x += (dc->rect.width / 2 - ww / 2);
        } else if (dc->align == UI_ALIGN_RIGHT) {
            x += (dc->rect.width - ww);
        }
        y += (dc->rect.height / 2 - hh / 2);
        while ((id != 0x00ff) && (id != 0xffff)) {
            if (open_image_by_id(NULL, &file, id, dc->page) != 0) {
                return -EFAULT;
            }

            disp.left   = x;
            disp.top    = y;
            disp.width  = file.width;
            disp.height = file.height;

            if (dc->data_format == DC_DATA_FORMAT_MONO) {
                pixelbuf = dc->fbuf;
                if (get_rect_cover(&draw_r, &disp, &r)) {
                    int _offset = -1;
                    for (h = 0; h < r.height; h++) {
                        if (file.compress == 0) {
                            int offset = (r.top + h - disp.top) / 8 * file.width + (r.left - disp.left);
                            if (_offset != offset) {
                                if (br23_read_image_data(NULL, &file, pixelbuf, r.width, offset) != r.width) {
                                    return -EFAULT;
                                }
                                _offset = offset;
                            }
                        } else {
                            ASSERT(0, "the compress mode not support!");
                        }
                        for (w = 0; w < r.width; w++) {
                            u8 color = (pixelbuf[w] & BIT((r.top + h - disp.top) % 8)) ? 1 : 0;
                            if (color) {
                                if (platform_api->draw_point) {
                                    u16 text_color;
                                    if (text->color != 0xffffff) {
                                        text_color = (text->color != UI_RGB565(TEXT_MONO_CLR)) ? (text->color ? text->color : 0xffff) : 0x55aa;
                                    } else {
                                        text_color = color;
                                    }
                                    platform_api->draw_point(dc, r.left + w, r.top + h, text_color);
                                }
                            }
                        }
                    }
                }
            } else if (dc->data_format == DC_DATA_FORMAT_OSD16) {
                int temp_pixelbuf_len = dc->width * 2 * dc->lines + 0x40 - 8;
                int temp_alphabuf_len = dc->width * dc->lines + 0x40 - 8;
                int buf_offset;

                buf_offset = 0;
                pixelbuf = &dc->fbuf[buf_offset];//2 bytes * line
                buf_offset += dc->width * 2;
                buf_offset = (buf_offset + 3) / 4 * 4;
                alphabuf = &dc->fbuf[buf_offset];//1 bytes * line
                buf_offset += dc->width;
                buf_offset = (buf_offset + 3) / 4 * 4;
                temp_pixelbuf = &dc->fbuf[buf_offset];
                buf_offset += temp_pixelbuf_len;
                buf_offset = (buf_offset + 3) / 4 * 4;
                temp_alphabuf = &dc->fbuf[buf_offset];

                u32 alpha_addr = 0;
                if (get_rect_cover(&draw_r, &disp, &r)) {
                    for (h = 0; h < r.height;) {
                        int vh = r.top + h - disp.top;
                        int vw = r.left - disp.left;

                        struct rle_line *line;
                        struct rle_line *alpha_line;
                        u8 *ptemp;
                        u8 *alpha_ptemp;

                        if (file.compress == 0) {
                            int offset = 4 + vh * file.width * 2 + vw * 2;
                            if (br23_read_image_data(NULL, &file, pixelbuf, r.width * 2, offset) != r.width * 2) {
                                return -EFAULT;
                            }
                            br23_read_image_data(NULL, &file, &alpha_addr, sizeof(alpha_addr), 0);
                            if (alpha_addr) {
                                offset = alpha_addr + vh * file.width + vw;
                                br23_read_image_data(NULL, &file, alphabuf, r.width, offset);
                            }
                        } else if (file.compress == 1) {
                            int remain = (r.height - h) > (file.height - vh) ? (file.height - vh) : (r.height - h);
                            int headlen = sizeof(struct rle_header) + (remain * 2 + 3) / 4 * 4;

                            line = (struct rle_line *)temp_pixelbuf;
                            ptemp = &temp_pixelbuf[headlen];
                            memset(line, 0x00, sizeof(struct rle_line));

                            int rle_header_len = sizeof(struct rle_header) * remain;
                            br23_read_image_data(NULL, &file, ptemp, rle_header_len, 4 + vh * sizeof(struct rle_header));

                            int i;
                            struct rle_header *rle = (struct rle_header *)ptemp;
                            int total_len = 0;
                            for (i = 0; i < remain; i++) {
                                if (i == 0) {
                                    line->addr = rle[i].addr;
                                    line->len[i] = rle[i].len;
                                } else {
                                    line->len[i] = rle[i].len;
                                }
                                if ((total_len + rle[i].len) > (temp_pixelbuf_len - headlen)) {
                                    break;
                                }
                                total_len += rle[i].len;
                                line->num ++;
                            }

                            br23_read_image_data(NULL, &file, ptemp, total_len, 4 + line->addr);

                            br23_read_image_data(NULL, &file, &alpha_addr, sizeof(alpha_addr), 0);
                            if (alpha_addr) {
                                int headlen = sizeof(struct rle_header) + (line->num * 2 + 3) / 4 * 4;
                                alpha_ptemp = &temp_alphabuf[headlen];
                                br23_read_image_data(NULL, &file, alpha_ptemp, sizeof(struct rle_header)*line->num, alpha_addr + vh * sizeof(struct rle_header));

                                struct rle_header *rle = (struct rle_header *)alpha_ptemp;
                                alpha_line = (struct rle_line *)temp_alphabuf;
                                memset(alpha_line, 0x00, sizeof(struct rle_line));
                                int total_len = 0;
                                for (i = 0; i < line->num; i++) {
                                    if (i == 0) {
                                        alpha_line->addr = rle[i].addr;
                                        alpha_line->len[i] = rle[i].len;
                                    } else {
                                        alpha_line->len[i] = rle[i].len;
                                    }
                                    if ((total_len + rle[i].len) > (temp_alphabuf_len - headlen)) {
                                        break;
                                    }
                                    total_len += rle[i].len;
                                    alpha_line->num ++;
                                }

                                br23_read_image_data(NULL, &file, alpha_ptemp, total_len, alpha_addr + alpha_line->addr);
                            }
                        } else {
                            ASSERT(0, "the compress mode not support!");
                        }

                        u8 *p0 = ptemp;
                        u8 *p1 = alpha_ptemp;
                        int line_num;
                        if (file.compress == 0) {
                            line_num = 1;
                        } else {
                            line_num = (line->num > alpha_line->num) ? alpha_line->num : line->num;
                        }

                        for (hh = 0; hh < line_num; hh++, h++) {
                            if (file.compress == 1) {
                                Rle_Decode(p0, line->len[hh], pixelbuf, file.width * 2, vw * 2, r.width * 2, 2);
                                p0 += line->len[hh];
                                Rle_Decode(p1, alpha_line->len[hh], alphabuf, file.width, vw, r.width, 1);
                                p1 += alpha_line->len[hh];
                            }
                            u16 *pdisp = (u16 *)dc->buf;
                            u16 *pixelbuf16 = (u16 *)pixelbuf;
                            for (w = 0; w < r.width; w++) {
                                u16 color, pixel;
                                u8  alpha = alpha_addr ? alphabuf[w] : 255;

                                pixel = color = pixelbuf16[w];
                                if (color) {
                                    if (platform_api->draw_point) {
                                        int vww = w;
                                        u16 x0 = r.left + vww;
                                        u16 y0 = r.top + h;

                                        if (alpha < 255) {
                                            u16 backcolor = platform_api->read_point(dc, x0, y0);
                                            pixel = get_mixed_pixel(backcolor >> 8 | (backcolor & 0xff) << 8, color>>8 | (color & 0xff) << 8, alpha);
                                        }

                                        int offset = (y0 - dc->disp.top) * dc->disp.width + (x0 - dc->disp.left);
                                        if ((offset * 2 + 1) < dc->len) {
                                            pdisp[offset] = pixel;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            x += file.width;
            cnt += 2;
            id = ((u8)text->str[cnt + 1] << 8) | (u8)text->str[cnt];
        }
    }

    return 0;
}
AT_UI_RAM
u32 jlui_read_point(struct draw_context *dc, u16 x, u16 y)
{
    u32 pixel;
    u16 *pdisp = dc->buf;
    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        int offset = (y - dc->disp.top) * dc->disp.width + (x - dc->disp.left);
        ASSERT((offset * 2 + 1) < dc->len, "dc->len:%d", dc->len);
        if ((offset * 2 + 1) >= dc->len) {
            return -1;
        }

        pixel = pdisp[offset];//(dc->buf[offset * 2] << 8) | dc->buf[offset * 2 + 1];
    } else {
        ASSERT(0);
    }

    return pixel;
}

__attribute__((always_inline_when_const_args))
AT_UI_RAM
int jlui_draw_point(struct draw_context *dc, u16 x, u16 y, u32 pixel)
{
    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        int offset = (y - dc->disp.top) * dc->disp.width + (x - dc->disp.left);

        /* ASSERT((offset * 2 + 1) < dc->len, "dc->len:%d", dc->len); */
        if ((offset * 2 + 1) >= dc->len) {
            return -1;
        }

        dc->buf[offset * 2    ] = pixel >> 8;
        dc->buf[offset * 2 + 1] = pixel;
    } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
        /* ASSERT(x < __this->info.width); */
        /* ASSERT(y < __this->info.height); */
        if ((x >= __this->info.width) || (y >= __this->info.height)) {
            return -1;
        }

        if (pixel & BIT(31)) {
            dc->buf[y / 8 * __this->info.width + x] ^= BIT(y % 8);
        } else if (pixel == 0x55aa) {
            dc->buf[y / 8 * __this->info.width + x] &= ~BIT(y % 8);
        } else if (pixel) {
            dc->buf[y / 8 * __this->info.width + x] |= BIT(y % 8);
        } else {
            dc->buf[y / 8 * __this->info.width + x] &= ~BIT(y % 8);
        }
    }

    return 0;
}

#endif


