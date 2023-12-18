/* COPYRIGHT NOTICE
 * 文件名称 ：ui_resources_manager.c
 * 简    介 ：ui资源和内存管理层
 * 功    能 ：
 * 			负责封装UI资源读写、UI框架使用的内存申请、释放等接口；
 * 			封装完后由ui_platform.c调用，再封装成UI框架的功能模块使用
 * 作    者 ：zhuhaifang
 * 创建时间 ：2022/04/26 20:22
 */
#include "ui/includes.h"
#include "timer.h"
#include "ui/lcd_spi/lcd_drive.h"
#include "ui/res_config.h"
#include "app_task.h"



#define WATCH_ITEMS_LIMIT		40


struct window_head {
    u32 offset;
    u32 len;
    u32 ptr_table_offset;
    u16 ptr_table_len;
    u16 crc_data;
    u16 crc_table;
    u16 crc_head;
};


struct ui_file_head {
    u8  res[16];
    u8  type;
    u8  window_num;
    u16 prop_len;
    u8  rotate;
    u8  rev[3];
};


static RESFILE *ui_file = NULL;
static RESFILE *ui_file1 = NULL;
static RESFILE *ui_file2 = NULL;
static int ui_file_len = 0;
static u32 ui_rotate = false;
static u32 ui_hori_mirror = false;
static u32 ui_vert_mirror = false;

static u8 watch_style;
static char *watch_res[WATCH_ITEMS_LIMIT] = {0};

static struct ui_priv {
    int window_offset;
};
static struct ui_priv priv ALIGNED(4);
#define __this (&priv)



/* UI buffer使用记录 */
/* #define UI_BUF_CALC */
#ifdef UI_BUF_CALC
struct buffer {
    struct list_head list;
    u8  *buf;	// buffer地址
    int size;	// buffer大小
    int type;	// buffer类型
};
static struct buffer *buffer_used = NULL;
#endif

static int malloc_cnt = 0;


void *jlui_malloc(int size)
{
    void *buf;
    malloc_cnt++;
    buf = (void *)malloc(size);

#ifdef UI_BUF_CALC
    if (!buffer_used) {
        /* 链表头，一次申请后将不再释放，发布版SDK无需使能buffer记录 */
        buffer_used = (struct buffer *)malloc(sizeof(struct buffer));
        INIT_LIST_HEAD(&buffer_used->list);
    }
    struct buffer *new = (struct buffer *)malloc(sizeof(struct buffer));
    new->buf = buf;
    new->size = size;
    list_add_tail(new, buffer_used);
    printf("platform_malloc : 0x%x, %d\n", buf, size);

    struct buffer *p;
    int buffer_used_total = 0;
    list_for_each_entry(p, &buffer_used->list, list) {
        buffer_used_total += p->size;
    }
    printf("used buffer size:%d\n\n", buffer_used_total);
#endif

    return buf;
}


void jlui_free(void *buf)
{
    free(buf);
    malloc_cnt--;

#ifdef UI_BUF_CALC
    struct buffer *p, *n;
    list_for_each_entry_safe(p, n, &buffer_used->list, list) {
        if (p->buf == buf) {
            printf("platform_free : 0x%x, %d\n", p->buf, p->size);
            __list_del_entry(p);
            free(p);
        }
    }

    int buffer_used_total = 0;
    list_for_each_entry(p, &buffer_used->list, list) {
        buffer_used_total += p->size;
    }
    printf("used buffer size:%d\n\n", buffer_used_total);
#endif
}



static int ui_platform_ok()
{
    return (malloc_cnt == 0);
}


int ui_style_file_version_compare(int version)
{
    int v;
    int len;
    struct ui_file_head head;
    static u8 checked = 0;

    if (checked == 0) {
        if (!ui_file) {
            puts("ui version_compare ui_file null!\n");
            ASSERT(0);
            return 0;
        }
        res_fseek(ui_file, 0, SEEK_SET);
        len = sizeof(struct ui_file_head);
        res_fread(ui_file, &head, len);
        printf("style file version is: 0x%x,UI_VERSION is: 0x%x\n", *(u32 *)(head.res), version);
        if (*(u32 *)head.res != version) {
            puts("style file version is not the same as UI_VERSION !!\n");
            ASSERT(0);
        }
        checked = 1;
    }
    return 0;
}

u32 __attribute__((weak)) set_retry_cnt()
{
    return 10;
}


int open_resource_file()
{
    int ret;

    printf("open_resouece_file...\n");
    FILE *fp = fopen(RES_PATH"JL/JL.res", "r");
    if (fp) {
        printf("open success: 0x%x\n", fp);
        fclose(fp);
    } else {
        printf("open faild!");
    }

    ret = open_resfile(RES_PATH"JL/JL.res");
    if (ret) {
        return -EINVAL;
    }
    ret = open_str_file(RES_PATH"JL/JL.str");
    if (ret) {
        return -EINVAL;
    }
    ret = font_ascii_init(FONT_PATH"ascii.res");
    if (ret) {
        return -EINVAL;
    }
    return 0;
}


int jlui_load_style(struct ui_style *style)
{
    int err;
    int i, j;
    int len;
    struct vfscan *fs;
    char name[64];
    char style_name[16];
    static char cur_style = 0xff;

    if (!style->file && cur_style == 0) {
        return 0;
    }

    if (ui_file1) {
        res_fclose(ui_file1);
        ui_file1 = NULL;
    }

    /* ASSERT(style->file); */
    if (style->file == NULL) {
        cur_style = 0;
        err = open_resource_file();
        if (err) {
            return -EINVAL;
        }
        ui_file1 = res_fopen(RES_PATH"JL/JL.sty", "r");
        if (!ui_file1) {
            return -ENOENT;
        }
        ui_file = ui_file1;
        ui_file_len = res_flen(ui_file1);
        len = 6;
        strcpy(style_name, "JL.sty");
        if (len) {
            style_name[len - 4] = 0;
            ui_core_set_style(style_name);
        }
    } else {
        cur_style = 1;

        printf("open stylefile %s\n", style->file);
        ui_file1 = res_fopen(style->file, "r");
        ASSERT(ui_file1);
        if (!ui_file1) {
            return -EINVAL;
        }


        /* if (!ui_file2) { */
        /*     ui_file2 = fopen(RES_PATH"sidebar/sidebar.sty", "r"); */
        /* } */

        ui_file = ui_file1;

        ui_file_len = 0xffffffff;//res_flen(ui_file1);
        for (i = 0; style->file[i] != '.'; i++) {
            name[i] = style->file[i];
        }
        printf("open resfile 0 :%s\n", name);
        name[i++] = '.';
        name[i++] = 'r';
        name[i++] = 'e';
        name[i++] = 's';
        name[i] = '\0';
        printf("open resfile %s\n", name);
        open_resfile(name);

        name[--i] = 'r';
        name[--i] = 't';
        name[--i] = 's';
        printf("open strfile %s\n", name);
        open_str_file(name);

        name[i++] = 'a';
        name[i++] = 's';
        name[i++] = 'i';
        printf("open asciifile %s\n", name);
        if (!strcmp(RES_PATH, EXTERN_PATH)) {
            font_ascii_init(RES_PATH"font/ascii.res");
        } else {
            font_ascii_init(RES_PATH"ascii.res");
        }

        for (i = strlen(style->file) - 5; i >= 0; i--) {
            if (style->file[i] == '/') {
                break;
            }
        }

        for (i++, j = 0; style->file[i] != '\0'; i++) {
            if (style->file[i] == '.') {
                name[j] = '\0';
                break;
            }
            name[j++] = style->file[i];
        }
        ASCII_ToUpper(name, j);
        if (!strncmp(name, "WATCH", strlen("WATCH"))) {
            strcpy(name, "JL");
        }
        err = ui_core_set_style(name);
        if (err) {
            printf("style_err: %s\n", name);
        }
    }

    return 0;

__err2:
    close_resfile();

__err1:
    res_fclose(ui_file1);
    ui_file1 = NULL;

    return err;
}



void *jlui_load_window(int id)
{
    u8 *ui;
    int i;
    u32 *ptr;
    u16 *ptr_table;
    struct ui_file_head head ALIGNED(4);
    struct window_head window ALIGNED(4);
    int len = sizeof(struct ui_file_head);
    int retry;
    static const int rotate[] = {0, 90, 180, 270};

    if (!ui_file) {
        printf("ui_file : 0x%x\n", ui_file);
        return NULL;
    }
    ui_platform_ok();

    for (retry = 0; retry < set_retry_cnt(); retry++) {
        res_fseek(ui_file, 0, SEEK_SET);
        res_fread(ui_file, &head, len);

        if (id >= head.window_num) {
            return NULL;
        }

        res_fseek(ui_file, sizeof(struct window_head)*id, SEEK_CUR);
        res_fread(ui_file, &window, sizeof(struct window_head));

        u16 crc = CRC16(&window, (u32) & (((struct window_head *)0)->crc_data));
        if (crc == window.crc_head) {
            ui_rotate = rotate[head.rotate];
            ui_core_set_rotate(ui_rotate);
            switch (head.rotate) {
            case 1: /* 旋转90度 */
                ui_hori_mirror = true;
                ui_vert_mirror = false;
                break;
            case 3:/* 旋转270度 */
                ui_hori_mirror = false;
                ui_vert_mirror = true;
                break;
            default:
                ui_hori_mirror = false;
                ui_vert_mirror = false;
                break;
            }
            goto __read_data;
        }
    }

    return NULL;

__read_data:
    /* ui = (u8 *)__this->api->malloc(window.len); */
    ui = (u8 *)jlui_malloc(window.len);
    if (!ui) {
        return NULL;
    }
    for (retry = 0; retry < set_retry_cnt(); retry++) {
        res_fseek(ui_file, window.offset, SEEK_SET);
        res_fread(ui_file, ui, window.len);

        u16 crc = CRC16(ui, window.len);
        if (crc == window.crc_data) {
            goto __read_table;
        }
    }

    /* __this->api->free(ui); */
    jlui_free(ui);
    return NULL;

__read_table:
    /* ptr_table = (u16 *)__this->api->malloc(window.ptr_table_len); */
    ptr_table = (u16 *)jlui_malloc(window.ptr_table_len);
    if (!ptr_table) {
        /* __this->api->free(ui); */
        jlui_free(ui);
        return NULL;
    }
    for (retry = 0; retry < set_retry_cnt(); retry++) {
        res_fseek(ui_file, window.ptr_table_offset, SEEK_SET);
        res_fread(ui_file, ptr_table, window.ptr_table_len);

        u16 crc = CRC16(ptr_table, window.ptr_table_len);
        if (crc == window.crc_table) {
            u16 *offset = ptr_table;
            for (i = 0; i < window.ptr_table_len; i += 2) {
                ptr = (u32 *)(ui + *offset++);
                if (*ptr != 0) {
                    *ptr += (u32)ui;
                }
            }
            /* __this->api->free(ptr_table); */
            jlui_free(ptr_table);
            return ui;
        }
    }

    /* __this->api->free(ui); */
    /* __this->api->free(ptr_table); */
    jlui_free(ui);
    jlui_free(ptr_table);

    return NULL;
}


void jlui_unload_window(void *ui)
{
    if (ui) {
        /* __this->api->free(ui); */
        jlui_free(ui);
    }
}


AT_UI_RAM
void *jlui_load_widget_info(void *__head, u8 page)
{
    struct ui_file_head head ALIGNED(4);
    static union ui_control_info info ALIGNED(4) = {0};
    static const int rotate[] = {0, 90, 180, 270};
    static int last_page = -1;
    int head_len;
    int retry = 10;

    if (page != (u8) - 1) {
        struct ui_style style = {0};

#if UI_WATCH_RES_ENABLE
        int load_style = 0;
        if (last_page == -1) { //第一次上电未初始化
            if (page == 0) { //表盘界面
                style.file = watch_res[watch_style];
            } else {//其他界面
                style.file = RES_PATH"JL/JL.sty";
            }
            load_style = 1;
        } else if ((page == 0) && (last_page > 0)) { //其他 -> 表盘
            style.file = watch_res[watch_style];
            load_style = 1;
        } else if ((page > 0) && (last_page == 0)) {//表盘 -> 其他
            style.file = RES_PATH"JL/JL.sty";
            load_style = 1;
        }

        last_page = page;
#if UI_UPGRADE_RES_ENABLE//升级模式加载资源
        if (app_get_curr_task() == APP_WATCH_UPDATE_TASK) {
            style.file = UPGRADE_PATH"upgrade.sty";
            load_style = 1;
            last_page = -1;//下次能重新加载资源
        }
#endif

#else
        int load_style = 1;
        last_page = page;
#endif

        if (load_style && platform_api->load_style) {
            platform_api->load_style(&style);
            load_style = 0;
        }

        /* last_page = page; */
        res_fseek(ui_file, 0, SEEK_SET);
        res_fread(ui_file, &head, sizeof(struct ui_file_head));
        ui_rotate = rotate[head.rotate];
        ui_core_set_rotate(ui_rotate);
        switch (head.rotate) {
        case 1: /* 旋转90度 */
            ui_hori_mirror = true;
            ui_vert_mirror = false;
            break;
        case 3:/* 旋转270度 */
            ui_hori_mirror = false;
            ui_vert_mirror = true;
            break;
        default:
            ui_hori_mirror = false;
            ui_vert_mirror = false;
            break;
        }

        res_fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, SEEK_SET);
        res_fread(ui_file, &__this->window_offset, sizeof(__this->window_offset));
    }

    ASSERT((u32)__head <= ui_file_len, ",__head invalid! __head : 0x%x ui_file_len : 0x%x\n", (u32)__head, ui_file_len);

    if ((u32)__head > ui_file_len) {
        return NULL;
    }

    res_fseek(ui_file, __this->window_offset + (u32)__head, SEEK_SET);
    if ((u32)__head == 0) {
        res_fread(ui_file, &info, sizeof(struct window_info));
    } else {
        res_fread(ui_file, &info.head, sizeof(struct ui_ctrl_info_head));
        head_len = info.head.len - sizeof(struct ui_ctrl_info_head);
        if ((head_len > 0) && (head_len <= sizeof(union ui_control_info))) {
            res_fread(ui_file, &((u8 *)&info)[sizeof(struct ui_ctrl_info_head)], head_len);
        }
    }

    return &info;
#if 0
    struct ui_file_head head ALIGNED(4);
    static union ui_control_info info ALIGNED(4) = {0};
    static const int rotate[] = {0, 90, 180, 270};
    static int last_page = -1;
    int head_len;
    int retry = 10;
    u32 offset;
    u32 _head = ((u32)__head) & 0xffff;

    if (!_head) {
        struct ui_style style = {0};

#if UI_WATCH_RES_ENABLE
        int load_style = 0;
        if (last_page == -1) { //第一次上电未初始化
            if (page == 0) { //表盘界面
                style.file = watch_res[watch_style];
            } else {//其他界面
                if (!strcmp(RES_PATH, EXTERN_PATH)) {
                    style.file = RES_PATH"JL/JL.sty";
                } else {
                    style.file = RES_PATH"JL.sty";
                }
            }
            load_style = 1;
        } else if ((page == 0) && (last_page == 0)) { // 表盘 -> 表盘
            style.file = watch_res[watch_style];
            load_style = 1;
        } else if ((page == 0) && (last_page > 0)) { //其他 -> 表盘
            style.file = watch_res[watch_style];
            load_style = 1;
        } else if ((page > 0) && (last_page == 0)) {//表盘 -> 其他
            if (!strcmp(RES_PATH, EXTERN_PATH)) {
                style.file = RES_PATH"JL/JL.sty";
            } else {
                style.file = RES_PATH"JL.sty";
            }
            load_style = 1;
        } else if ((page == 74) || (last_page == 74)) { //特殊情况处理
            //公版sdk的PAGE_74存在动态加载侧边栏操作，每次进入或退出该页面需重新更新ui_file，否则异常
            //如果客户开发时不存在该页面或者该页面没有动态加载侧边栏操作，可注释该else if
            mem_stats();
            if (!strcmp(RES_PATH, EXTERN_PATH)) {
                style.file = RES_PATH"JL/JL.sty";
            } else {
                style.file = RES_PATH"JL.sty";
            }
            load_style = 1;
        }

        last_page = page;
#if UI_UPGRADE_RES_ENABLE//升级模式加载资源
        if (app_get_curr_task() == APP_WATCH_UPDATE_TASK ||
            app_get_curr_task() == APP_SMARTBOX_ACTION_TASK) {
            ui_set_sty_path_by_pj_id(1, NULL);
            style.file = UPGRADE_PATH"upgrade.sty";
            load_style = 1;
            last_page = -1;//下次能重新加载资源
            if (page) {
                printf("NOW IS UPGRADE MODE ,CAN NOT SHOW OTHER PAGE...\n");
                page = 0;
            }
        }
#endif

#else
        int load_style = 1;
        last_page = page;
#endif

        if (load_style && platform_api->load_style) {
            if (platform_api->load_style(&style)) {
                return NULL;
            }
            load_style = 0;
        }
        if (!ui_file) {
            return NULL;
        }

        /* last_page = page; */
        res_fseek(ui_file, 0, SEEK_SET);
        res_fread(ui_file, &head, sizeof(struct ui_file_head));
        ui_rotate = rotate[head.rotate];
        ui_core_set_rotate(ui_rotate);
        switch (head.rotate) {
        case 1: /* 旋转90度 */
            ui_hori_mirror = true;
            ui_vert_mirror = false;
            break;
        case 3:/* 旋转270度 */
            ui_hori_mirror = false;
            ui_vert_mirror = true;
            break;
        default:
            ui_hori_mirror = false;
            ui_vert_mirror = false;
            break;
        }
        if (page != (u8) - 1) {
            res_fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, SEEK_SET);
            res_fread(ui_file, &__this->window_offset, sizeof(__this->window_offset));
        }
    }

    if ((((u32)__head >> 29) & 0x7)) {
        if ((((u32)__head >> 29) & 0x7) == 1) {
            /* printf("%s \n",watch_res[watch_style]); */
            ui_set_sty_path_by_pj_id(1, watch_res[watch_style]);
            /* printf(" >>>>>>>>%s %d %x\n",__FUNCTION__,__LINE__,(int)__head >> 29); */
        }
        ui_file = ui_load_sty_by_pj_id((((u32)__head >> 29) & 0x7));

    } else {
        ASSERT(ui_file1);
        ui_file = ui_file1;
    }
    ASSERT(ui_file);

    ASSERT((u32)_head <= ui_file_len, ",_head invalid! _head : 0x%x ui_file_len : 0x%x\n", (u32)_head, ui_file_len);

    if ((u32)_head > ui_file_len) {
        return NULL;
    }

    if ((u32)__head >= 0x10000) {
        page = ((u32)__head >> 22) & 0x7f;

        struct mem_var_list *list;
        if ((list = mem_var_search(1, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head) * page, page, (((u32)__head >> 29) & 0x7))) != NULL) {
            mem_var_get(list, &offset, sizeof(__this->window_offset));
        } else {
            res_fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, SEEK_SET);
            res_fread(ui_file, &offset, sizeof(__this->window_offset));
            mem_var_add(1, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, page, (((u32)__head >> 29) & 0x7), &offset, sizeof(__this->window_offset));
        }
    } else {
        offset = __this->window_offset;
    }

    res_fseek(ui_file, offset + (u32)_head, SEEK_SET);
    if ((u32)_head == 0) {
        res_fread(ui_file, &info, sizeof(struct window_info));
    } else {
        res_fread(ui_file, &info.head, sizeof(struct ui_ctrl_info_head));
        head_len = info.head.len - sizeof(struct ui_ctrl_info_head);
        if ((head_len > 0) && (head_len <= sizeof(union ui_control_info))) {
            res_fread(ui_file, &((u8 *)&info)[sizeof(struct ui_ctrl_info_head)], head_len);
        }
    }

    return &info;
#endif
}



AT_UI_RAM
void *jlui_load_css(u8 page, void *_css)
{
    static struct element_css1 css ALIGNED(4) = {0};
    u32 offset;

    if ((((u32)_css >> 29) & 0x7)) {
        ui_file = ui_load_sty_by_pj_id((((u32)_css >> 29) & 0x7));
    } else {
        ui_file = ui_file1;
    }

    if ((u32)_css >= 0x10000) {
        page = ((u32)_css >> 22) & 0x7f;
    }
    struct mem_var_list *list;
    if ((list = mem_var_search(3, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head) * page, page, (((u32)_css >> 29) & 0x7))) != NULL) {
        mem_var_get(list, &offset, sizeof(offset));
    } else {
        res_fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, SEEK_SET);
        res_fread(ui_file, &offset, sizeof(offset));
        mem_var_add(3, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, page, (((u32)_css >> 29) & 0x7), &offset, sizeof(offset));
    }
    ASSERT((u32)_css <= ui_file_len, ", _css invalid! _css : 0x%x , ui_file_len : 0x%x\n", _css, ui_file_len);

    res_fseek(ui_file, offset/* __this->window_offset */ + ((u32)_css & 0xffff), SEEK_SET);
    res_fread(ui_file, &css, sizeof(struct element_css1));

    return &css;
}



AT_UI_RAM
void *jlui_load_image_list(u8 page, void *_list)
{
    /* u16 image[32] ALIGNED(4); */
    static struct ui_image_list_t list ALIGNED(4) = {0};
    int retry = 10;
    u32 offset;

    /* printf(" >>>>>>>>%s %d %x\n",__FUNCTION__,__LINE__,(int)_list >> 29); */
    if ((((u32)_list >> 29) & 0x7)) {
        ui_file = ui_load_sty_by_pj_id((((u32)_list >> 29) & 0x7));
    } else {
        ui_file = ui_file1;
    }
    if ((u32)_list >= 0x10000) {
        page = ((u32)_list >> 22) & 0x7f;
    }

    struct mem_var_list *mem_list;
    if ((mem_list = mem_var_search(4, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head) * page, page, (((u32)_list >> 29) & 0x7))) != NULL) {
        mem_var_get(mem_list, &offset, sizeof(offset));
    } else {
        res_fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, SEEK_SET);
        res_fread(ui_file, &offset, sizeof(offset));
        mem_var_add(4, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, page, (((u32)_list >> 29) & 0x7), &offset, sizeof(offset));
    }
    if ((u32)_list == 0) {
        return NULL;
    }

    ASSERT((u32)_list <= ui_file_len, ", _list invalid! _list : 0x%x, ui_file_len : 0x%x\n", (u32)_list, ui_file_len);

    do {
        memset(&list, 0x00, sizeof(struct ui_image_list));

        struct mem_var_list *mem_list;
        if ((mem_list = mem_var_search(5, (u32)ui_file, offset + ((u32)_list & 0xffff), page, (((u32)_list >> 29) & 0x7))) != NULL) {
            mem_var_get(mem_list, &list.num, sizeof(list.num));
        } else {
            res_fseek(ui_file, offset + ((u32)_list & 0xffff), SEEK_SET);
            res_fread(ui_file, &list.num, sizeof(list.num));
            mem_var_add(5, (u32)ui_file, offset + ((u32)_list & 0xffff), page, (((u32)_list >> 29) & 0x7), &list.num, sizeof(list.num));
        }

        /* res_fseek(ui_file, offset + ((u32)_list & 0xffff), SEEK_SET); */
        /* res_fread(ui_file, &list.num, sizeof(list.num)); */
        if (list.num == 0) {
            printf("list.num : %d\n", list.num);
            return NULL;
        }
        if (list.num < 32) {
            res_fseek(ui_file, offset + ((u32)_list & 0xffff) + sizeof(list.num), SEEK_SET);
            res_fread(ui_file, list.image, list.num * sizeof(list.image[0]));
            /* memcpy(list.image, image, list.num * sizeof(list.image[0])); */
            break;
        } else {
            printf("list.num = %d\n", list.num);
            printf("load_image_list error,retry %d!\n", retry);
            if (retry == 0) {
                return NULL;
            }
        }
    } while (retry--);

    return &list;
}


AT_UI_RAM
void *jlui_load_text_list(u8 page, void *__list)
{
    u8 buf[50 * 2] ALIGNED(4);
    static struct ui_text_list_t _list ALIGNED(4) = {0};
    struct ui_text_list_t *list;
    int retry = 10;
    int i;
    u32 offset;

    /* printf(" >>>>>>>>%s %d %x\n",__FUNCTION__,__LINE__,(u32)__list >> 29); */
    if ((((u32)__list >> 29) & 0x7)) {
        ui_file = ui_load_sty_by_pj_id((((u32)__list >> 29) & 0x7));
    } else {
        ui_file = ui_file1;
    }


    if (!ui_file) {
        return NULL;
    }


    if ((u32)__list >= 0x10000) {
        page = ((u32)__list >> 22) & 0x7f;
    }

    struct mem_var_list *mem_list;
    if ((mem_list = mem_var_search(7, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head) * page, page, (((u32)__list >> 29) & 0x7))) != NULL) {
        mem_var_get(mem_list, &offset, sizeof(offset));
    } else {
        res_fseek(ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, SEEK_SET);
        res_fread(ui_file, &offset, sizeof(offset));
        mem_var_add(7, (u32)ui_file, sizeof(struct ui_file_head) + sizeof(struct window_head)*page, page, (((u32)__list >> 29) & 0x7), &offset, sizeof(offset));
    }
    if ((u32)__list == 0) {
        return NULL;
    }

    ASSERT((u32)__list <= ui_file_len, ", __list invalid! _list : 0x%x, ui_file_len : 0x%x\n", (u32)__list, ui_file_len);

    list = &_list;
    do {
        memset(list, 0x00, sizeof(struct ui_text_list_t));

        struct mem_var_list *mem_list;
        if ((mem_list = mem_var_search(8, (u32)ui_file, offset + ((u32)__list & 0xffff), page, (((u32)__list >> 29) & 0x7))) != NULL) {
            mem_var_get(mem_list, &list->num, sizeof(list->num));
        } else {
            res_fseek(ui_file, offset + ((u32)__list & 0xffff), SEEK_SET);
            res_fread(ui_file, &list->num, sizeof(list->num));
            mem_var_add(8, (u32)ui_file, offset + ((u32)__list & 0xffff), page, (((u32)__list >> 29) & 0x7), &list->num, sizeof(list->num));
        }

        /* res_fseek(ui_file, offset + ((u32)__list & 0xffff), SEEK_SET); */
        /* res_fread(ui_file, &list->num, sizeof(list->num)); */
        if (list->num == 0) {
            return NULL;
        }
        if (list->num <= 50) {
            res_fseek(ui_file, offset + ((u32)__list & 0xffff) + sizeof(list->num), SEEK_SET);
            res_fread(ui_file, buf, list->num * 2);
            for (i = 0; i < list->num; i++) {
                ASSERT(buf[2 * i] < 0x100);
                list->str[i] = buf[2 * i] | (buf[2 * i + 1] << 8);
            }
            break;
        } else {
            printf("list->num = %d\n", list->num);
            printf("load_text_list error, retry %d!\n", retry);
            if (retry == 0) {
                return NULL;
            }
        }
    } while (retry--);
    return list;
}


int jlui_read_image_info(struct draw_context *dc, u32 id, u8 page, struct ui_image_attrs *attrs)
{
    struct image_file file = {0};

    if (((u16) - 1 == id) || ((u32) - 1 == id)) {
        return -1;
    }

    select_resfile(dc->prj);
    int err = open_image_by_id(NULL, &file, id, dc->page);
    if (err) {
        return -EFAULT;
    }
    attrs->width = file.width;
    attrs->height = file.height;

    return 0;
}




