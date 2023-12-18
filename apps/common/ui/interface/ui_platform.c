#include "timer.h"
#include "ui/includes.h"
#include "ui/lcd_spi/lcd_drive.h"
#include "ui/buffer_manager.h"


struct ui_priv {
    struct ui_platform_api *api;
    struct lcd_interface *lcd;
    int window_offset;
    struct lcd_info info;
};
static struct ui_priv priv ALIGNED(4);
#define __this (&priv)


static/*  const  */struct ui_platform_api br23_platform_api;



// ui_synthesis_manager.c
extern int jlui_fill_rect(struct draw_context *dc, u32 acolor);
extern int jlui_draw_rect(struct draw_context *dc, struct css_border *border);
extern int jlui_draw_image(struct draw_context *dc, u32 src, u8 quadrant, u8 *mask);
extern int jlui_show_text(struct draw_context *dc, struct ui_text_attrs *text);
extern u32 jlui_read_point(struct draw_context *dc, u16 x, u16 y);
extern int jlui_draw_point(struct draw_context *dc, u16 x, u16 y, u32 pixel);
extern int jlui_invert_rect(struct draw_context *dc, u32 acolor);
extern void platform_putchar(struct font_info *info, u8 *pixel, u16 width, u16 height, u16 x, u16 y);
extern void set_screen_info(struct lcd_info *info);


// ui_resources_manager.c
extern void *jlui_malloc(int size);
extern void jlui_free(void *buf);
extern int  jlui_load_style(struct ui_style *style);
extern void *jlui_load_window(int id);
extern void jlui_unload_window(void *ui);
extern void *jlui_load_widget_info(void *__head, u8 page);
extern void *jlui_load_css(u8 page, void *_css);
extern void *jlui_load_image_list(u8 page, void *_list);
extern void *jlui_load_text_list(u8 page, void *__list);
extern int  jlui_read_image_info(struct draw_context *dc, u32 id, u8 page, struct ui_image_attrs *attrs);
extern int  ui_platform_ok();




int get_cur_srreen_width_and_height(u16 *screen_width, u16 *screen_height)
{
    memcpy((u8 *)screen_width, (u8 *)&__this->info.width, sizeof(__this->info.width));
    memcpy((u8 *)screen_height, (u8 *)&__this->info.height, sizeof(__this->info.height));
    return 0;
}


static int jlui_open_draw_context(struct draw_context *dc)
{
    dc->buf_num = 1;
    if (__this->lcd->buffer_malloc) {
        u8 *buf;
        u32 len;
        __this->lcd->buffer_malloc(&buf, &len);
        printf("%s() buf: 0x%x\n", __FUNCTION__, buf);

        dc->buf0 = buf;
#if UI_USED_DOUBLE_BUFFER
        dc->buf1 = &buf[len / 2];
        dc->len  = len / 2;
#else
        dc->buf1 = NULL;
        dc->len  = len;
#endif
        dc->buf  = dc->buf0;
    }

    if (__this->lcd->get_screen_info) {
        __this->lcd->get_screen_info(&__this->info);
        dc->width		= __this->info.width;
        dc->height		= __this->info.height;
        dc->col_align	= __this->info.col_align;
        dc->row_align	= __this->info.row_align;
        // @@@@@ 临时调试12864黑白屏赋立即数
        dc->lines		= 64;//dc->len / dc->width / 2;
        /* if (dc->lines > (dc->height / 10)) { */
        /* dc->lines = dc->height / 10; */
        /* } */
        printf("dc->width : %d, dc->lines : %d\n", dc->width, dc->lines);
    }

    printf("dc->data_format: %d\n", dc->data_format);
    switch (__this->info.color_format) {
    case LCD_COLOR_RGB565:
        if (dc->data_format != DC_DATA_FORMAT_OSD16) {
            ASSERT(0, "The color format of layer don't match the lcd driver,page %d please select OSD16!", dc->page);
        }
        break;
    case LCD_COLOR_MONO:
        if (dc->data_format != DC_DATA_FORMAT_MONO) {
            ASSERT(0, "The color format of layer don't match the lcd driver,page %d please select OSD1!", dc->page);
        }
        break;
    }

    /* printf("location [[%s : %s : %d]]\n", __FILE__, __FUNCTION__, __LINE__); */
    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        dc->fbuf_len = __this->info.width * (3 + 3 * dc->lines) + 0x80;
        dc->fbuf = (u8 *)__this->api->malloc(dc->fbuf_len);
    } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
        dc->fbuf_len = __this->info.width * 2;
        dc->fbuf = (u8 *)__this->api->malloc(dc->fbuf_len);
    }

    return 0;
}

static int jlui_get_draw_context(struct draw_context *dc)
{
#if UI_USED_DOUBLE_BUFFER
    if (dc->buf == dc->buf0) {
        dc->buf = dc->buf1;
    } else {
        dc->buf = dc->buf0;
    }
#endif

    dc->disp.left  = dc->need_draw.left;
    dc->disp.width = dc->need_draw.width;
    if (dc->data_format == DC_DATA_FORMAT_OSD16) {
        int lines = dc->len / dc->need_draw.width / 2;

        if ((dc->disp.top == 0) && (dc->disp.height == 0)) {
            dc->disp.top   = dc->need_draw.top;
            dc->disp.height = lines > dc->need_draw.height ? dc->need_draw.height : lines;
        } else {
            dc->disp.top   = dc->disp.top + dc->disp.height;
            dc->disp.height = lines > (dc->need_draw.top + dc->need_draw.height - dc->disp.top) ?
                              (dc->need_draw.top + dc->need_draw.height - dc->disp.top) : lines;
        }
        dc->disp.height = dc->disp.height / dc->row_align * dc->row_align;
    } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
        dc->disp.top = dc->need_draw.top;
        dc->disp.height = dc->need_draw.height;
    }

    return 0;
}

static int jlui_put_draw_context(struct draw_context *dc)
{
    if (__this->lcd->set_draw_area) {
        __this->lcd->set_draw_area(dc->disp.left, dc->disp.left + dc->disp.width - 1,
                                   dc->disp.top, dc->disp.top + dc->disp.height - 1);
    }

    u8 wait = ((dc->need_draw.top + dc->need_draw.height) == (dc->disp.top + dc->disp.height)) ? 1 : 0;
    if (__this->lcd->draw) {
        if (dc->data_format == DC_DATA_FORMAT_OSD16) {
            __this->lcd->draw(dc->buf, dc->disp.height * dc->disp.width * 2, wait);
        } else if (dc->data_format == DC_DATA_FORMAT_MONO) {
            __this->lcd->draw(dc->buf, __this->info.width * __this->info.height / 8, wait);
        }
    }

    return 0;
}


static int jlui_set_draw_context(struct draw_context *dc)
{
    return 0;
}

static int jlui_close_draw_context(struct draw_context *dc)
{
    if (__this->lcd->buffer_free) {
        printf("%s() buf: 0x%x\n", __FUNCTION__, dc->buf);
        __this->lcd->buffer_free(dc->buf);
    }
    if (dc->fbuf) {
        __this->api->free(dc->fbuf);
        dc->fbuf = NULL;
        dc->fbuf_len = 0;
    }

    return 0;
}

static void *jlui_set_timer(void *priv, void (*callback)(void *), u32 msec)
{
    return (void *)sys_timer_add(priv, callback, msec);
}

static int jlui_del_timer(void *fd)
{
    if (fd) {
        sys_timer_del((int)fd);
    }
    return 0;
}

int jlui_open_device(struct draw_context *dc, const char *device)
{
    return 0;
}

int jlui_close_device(int fd)
{
    return 0;
}




static/*  const  */struct ui_platform_api br23_platform_api = {
    /* ui_resources_manager.c */
    .malloc             = jlui_malloc,
    .free               = jlui_free,
    .load_style         = jlui_load_style,
    .load_window        = jlui_load_window,
    .unload_window      = jlui_unload_window,
    .load_widget_info   = jlui_load_widget_info,
    .load_css           = jlui_load_css,
    .load_image_list    = jlui_load_image_list,
    .load_text_list     = jlui_load_text_list,
    .read_image_info    = jlui_read_image_info,

    /* ui_synthesis_manager.c */
    .fill_rect          = jlui_fill_rect,
    .draw_rect          = jlui_draw_rect,
    .draw_image         = jlui_draw_image,
    .show_text          = jlui_show_text,
    .read_point         = jlui_read_point,
    .draw_point         = jlui_draw_point,
    .invert_rect        = jlui_invert_rect,

    /* ui_platform.c */
    .open_draw_context  = jlui_open_draw_context,
    .get_draw_context   = jlui_get_draw_context,
    .put_draw_context   = jlui_put_draw_context,
    .set_draw_context   = jlui_set_draw_context,
    .close_draw_context = jlui_close_draw_context,
    .open_device        = jlui_open_device,
    .close_device       = jlui_close_device,
    .set_timer          = jlui_set_timer,
    .del_timer          = jlui_del_timer,

    /* None */
    .file_browser_open  = NULL,
    .get_file_attrs     = NULL,
    .set_file_attrs     = NULL,
    .show_file_preview  = NULL,
    .move_file_preview  = NULL,
    .clear_file_preview = NULL,
    .flush_file_preview = NULL,
    .open_file          = NULL,
    .delete_file        = NULL,
    .file_browser_close = NULL,
};



/* API NOTES
 * 名    称 ：int ui_platform_init(void *lcd)
 * 功    能 ：UI框架初始化，注意：所有必要的接口都在这里检查，如果不存在则直接进入断言，
 * 参    数 ：*lcd LCD配置
 * 返 回 值 ：
 */
int ui_platform_init(void *lcd)
{
    struct rect rect;
    struct lcd_info info = {0};

    __this->api = &br23_platform_api;
    ASSERT(__this->api->open_draw_context);
    ASSERT(__this->api->get_draw_context);
    ASSERT(__this->api->put_draw_context);
    ASSERT(__this->api->set_draw_context);
    ASSERT(__this->api->close_draw_context);


    __this->lcd = lcd_get_hdl();
    ASSERT(__this->lcd);
    ASSERT(__this->lcd->init);
    ASSERT(__this->lcd->get_screen_info);
    ASSERT(__this->lcd->buffer_malloc);
    ASSERT(__this->lcd->buffer_free);
    ASSERT(__this->lcd->draw);
    ASSERT(__this->lcd->set_draw_area);
    ASSERT(__this->lcd->backlight_ctrl);

    if (__this->lcd->init) {
        __this->lcd->init(lcd);
    }

    if (__this->lcd->backlight_ctrl) {
        __this->lcd->backlight_ctrl(true);
    }

    if (__this->lcd->get_screen_info) {
        __this->lcd->get_screen_info(&info);
        set_screen_info(&info);
    }

    if (__this->lcd->clear_screen) {
        __this->lcd->clear_screen(0xff0000);
        /* os_time_dly(100); */
        /* __this->lcd->clear_screen(0x00ff00); */
        /* os_time_dly(100); */
        /* __this->lcd->clear_screen(0x0000ff); */
    }

    rect.left   = 0;
    rect.top    = 0;
    rect.width  = info.width;
    rect.height = info.height;

    printf("ui_platform_init :: [%d,%d,%d,%d]\n", rect.left, rect.top, rect.width, rect.height);
    ui_core_init(__this->api, &rect);

    return 0;
}







