/* COPYRIGHT NOTICE
 * 文件名称 ：watch_bgp.c
 * 简    介 ：表盘管理代码
 * 功    能 ：
 *
 * 			1、原先表盘管理代码被放到ui_platform.c中，但并非属于UI接口
 *
 * 			暂时放到本文件，解决编译问题，后续放到合适的位置。
 *
 * 作    者 ：zhuhaifang
 * 创建时间 ：2022/05/13 15:31
 */
#include "ui/includes.h"
#include "timer.h"
#include "asm/crc16.h"
#include "ui/lcd_spi/lcd_drive.h"
#include "ascii.h"
#include "font/font_textout.h"
#include "res/rle.h"
#include "res/resfile.h"
#include "res/mem_var.h"
#include "ui/res_config.h"
#include "app_config.h"
#if (TCFG_OLED_SPI_SSD1306_ENABLE == 0)
#include "dev_manager.h"
#if SMART_BOX_EN
#include "smartbox/smartbox_task.h"
#endif
#if RCSP_MODE
#include "rcsp_task.h"
#endif
#endif
#include "app_task.h"
#include "fs/fs.h"
#include "ui/ui_image.h"


#define WATCH_ITEMS_LIMIT		40
#define BGP_ITEMS_LIMIT		    (WATCH_ITEMS_LIMIT + 10)//(确保 >= WATCH_ITEMS_LIMIT就可以)

static u8 watch_style;
static u8 watch_items;
static u8 watch_bgp_items;
static volatile u8 watch_update_over = 0;

static char *watch_res[WATCH_ITEMS_LIMIT] = {0};
static char *watch_bgp[BGP_ITEMS_LIMIT] = {0};
static char *watch_bgp_related[WATCH_ITEMS_LIMIT] = {0};

struct ui_load_info ui_load_info_table[] = {
#if UI_WATCH_RES_ENABLE
    {1, NULL, NULL},
    {2, RES_PATH"sidebar/sidebar.sty", NULL},
#endif
    {-1, NULL, NULL},
};




#if UI_WATCH_RES_ENABLE

char *watch_bgp_add(char *bgp)
{
    char *root_path = RES_PATH;
    char *new_bgp_item = NULL;
    u32 len;
    u32 i;

    if ((watch_bgp_items + 1) >= BGP_ITEMS_LIMIT) {
        printf("\n\n\nwatch_bgp items overflow %d\n\n\n\n", watch_bgp_items);
        return NULL;
    }

    len = strlen(bgp) + strlen(root_path) + 1;
    new_bgp_item = malloc(len);
    if (new_bgp_item == NULL) {
        printf("\n\n\nwatch_bgp items malloc err %d\n\n\n\n", len);
        return NULL;
    }

    ASCII_ToLower(bgp, strlen(bgp));
    strcpy(new_bgp_item, root_path);
    /* strcpy(&new_bgp_item[strlen(root_path)], &bgp[1]); */
    strcpy(&new_bgp_item[strlen(root_path)], bgp);
    new_bgp_item[len - 1] = '\0';

    //如果已经存在这个背景图片路径，则直接返回对应的地址
    for (i = 0; i < watch_bgp_items; i++) {
        /* printf("num %s\n", watch_bgp[i]); */
        if (strncmp(new_bgp_item, watch_bgp[i], strlen(new_bgp_item)) == 0) {
            free(new_bgp_item);
            new_bgp_item = watch_bgp[i];
            printf("already has %s\n", new_bgp_item);
            return new_bgp_item;
        }
    }

    watch_bgp[watch_bgp_items] = new_bgp_item;
    watch_bgp_items++;

    printf("add new_bgp_item succ %d, %s\n", watch_bgp_items, new_bgp_item);

    return new_bgp_item;
}

int watch_bgp_del(char *bgp)
{
    u32 i;
    char watch_bgp_item[64];
    u32 cur_items = watch_bgp_items;
    char *root_path = RES_PATH;
    char *bgp_item = NULL;

    ASSERT(((strlen(bgp) + strlen(root_path) + 1) < sizeof(watch_bgp_item)), "bgp err name0 %s\n", bgp);

    ASCII_ToLower(bgp, strlen(bgp));
    strcpy(watch_bgp_item, root_path);
    /* strcpy(&watch_bgp_item[strlen(root_path)], &bgp[1]); */
    strcpy(&watch_bgp_item[strlen(root_path)], bgp);
    watch_bgp_item[strlen(bgp) + strlen(root_path)] = '\0';
    printf("watch_bgp_item %s\n", watch_bgp_item);

    for (i = 0; i < cur_items; i++) {
        if (strncmp(watch_bgp_item, watch_bgp[i], strlen(watch_bgp_item)) == 0) {
            bgp_item = watch_bgp[i];
            watch_bgp[i] = NULL;
            free(bgp_item);
            watch_bgp_items--;
            break;
        }
    }

    if (bgp_item == NULL) {
        printf("can not find bgp_item %s\n", watch_bgp_item);
        return -1;
    }

    for (; i < cur_items; i++) {
        if (watch_bgp[i + 1] != NULL) {
            watch_bgp[i] = watch_bgp[i + 1];
        } else {
            watch_bgp[i] = NULL;
            break;
        }
    }

    //del related item
    cur_items = sizeof(watch_bgp_related) / sizeof(watch_bgp_related[0]);
    for (i = 0; i < cur_items; i++) {
        if (bgp_item == watch_bgp_related[i]) {
            watch_bgp_related[i] = NULL;
        }
    }

    for (i = 0; i < cur_items; i++) {
        printf("cur related items %d, %s\n", i, watch_bgp_related[i]);
    }
    for (i = 0; i < watch_bgp_items; i++) {
        printf("cur bgp items %d, %s\n", i, watch_bgp[i]);
    }

    return 0;
}


#define WATCH_MEM_BGP		0x55aa66bb
static u32 wmem_last = 0;
static u32 wmem_area_num = 0;
static void *wmem_file = NULL;


int watch_mem_new(u32 area)
{
    wmem_last = area;
    wmem_area_num++;
    return 0;
}

void *watch_mem_open()
{
    if (wmem_file == NULL) {
#if (WATCH_FILE_TO_FLASH)
#if (TCFG_NOR_FAT)
        wmem_file = fopen("storage/fat_nor/C/wmem.bin", "w+");
#else // TCFG_NOR_FAT
        wmem_file = fopen(RES_PATH"wmem.bin", "w+");
#endif // TCFG_NOR_FAT
#else // WATCH_FILE_TO_FLASH
        wmem_file = fopen("storage/sd1/C/download/wmem.bin", "w+");
#endif // WATCH_FILE_TO_FLASH
    }
    return wmem_file;
}

void watch_mem_close()
{
    if (wmem_file) {
        fclose(wmem_file);
        wmem_file = NULL;
    }
}

int watch_mem_write(u32 offset, u32 len, u8 *buf, u32 area)
{
    u32 ret;
    u8 tmp_buf[8];
    u32 area_offset = 0;
    u32 area_len = 0;
    u32 find_tag = 0;

    if (wmem_file == NULL) {
        return -1;
    }

    if ((flen(wmem_file) == 0) || (wmem_area_num <= 1)) {
        area_offset = 0;
    } else {
        do {
            fseek_fast(wmem_file, area_offset, SEEK_SET);
            ret = fread_fast(wmem_file, tmp_buf, 8);
            if (ret != 8) {
                printf("wmem find tag err end\n");
                return -1;
            }
            memcpy(&ret, tmp_buf + 4, 4); //flag
            if (ret == area) {
                memcpy(&area_len, tmp_buf, 4);//len
                find_tag = 1;
                break;
            }
            memcpy(&ret, tmp_buf, 4);//len
            area_offset += ret;
        } while (find_tag == 0);
    }

    fseek_fast(wmem_file, area_offset + offset, SEEK_SET);
    if (area == wmem_last) {
        ret = fwrite(wmem_file, buf, len);
        if (ret != len) {
            return -1;
        }
    } else {
        if ((offset + len) <= area_len) {
            ret = fwrite(wmem_file, buf, len);
            if (ret != len) {
                return -1;
            }
        } else {
            //要将这个区域的内容先搬迁到文件最后，使这个区域成为最后的区域
            //再写数据

        }
    }

    return 0;
}

/* static u32 wmem_test_flag = 0; */
int watch_mem_read(u32 offset, u32 len, u8 *buf, u32 area)
{
    u32 area_offset = 0;
    u32 area_len = 0;
    u32 find_tag = 0;
    u32 ret;
    u8 tmp_buf[8];

    if (wmem_file == NULL) {
        return -1;
    }

    if (flen(wmem_file) == 0) {
        /* wmem_test_flag = 1; */
        return 0;
    }


    if (wmem_area_num <= 1) {
        area_offset = 0;
    } else {
        do {
            fseek_fast(wmem_file, area_offset, SEEK_SET);
            ret = fread_fast(wmem_file, tmp_buf, 8);
            if (ret != 8) {
                printf("wmem find tag err end\n");
                return -1;
            }
            memcpy(&ret, tmp_buf + 4, 4); //flag
            if (ret == area) {
                memcpy(&area_len, tmp_buf, 4);//len
                find_tag = 1;
                break;
            }
            memcpy(&ret, tmp_buf, 4);//len
            area_offset += ret;
        } while (find_tag == 0);

        if ((offset + len) > area_len) {
            return -1;
        }
    }

    fseek_fast(wmem_file, area_offset + offset, SEEK_SET);
    ret = fread_fast(wmem_file, buf, len);
    if (ret != len) {
        printf("wmem read err %d\n", ret);
        return -1;
    }

    return 0;
}


static int watch_mem_bgp_related()
{
    int ret = 0;
    u8 *related_buf;
    u32 total_relate_items = sizeof(watch_bgp_related) / sizeof(watch_bgp_related[0]);
    u32 len;
    char *related_item;
    u32 area = WATCH_MEM_BGP;
    u32 i;

    if (watch_bgp_items == 0) {
        return -1;
    }

    len = 64 * (total_relate_items + 1);
    related_buf = zalloc(len);
    if (related_buf == NULL) {
        return -1;
    }

    if (watch_mem_open() == NULL) {
        free(related_buf);
        return -1;
    }

    memcpy(related_buf, &len, 4);
    memcpy(related_buf + 4, &area, 4);
    for (i = 0; i < total_relate_items; i++) {
        related_item = watch_bgp_related[i];
        if (related_item) {
            printf("related item : %s, %d, %d\n", related_item, strlen(related_item) + 1, i);
            memcpy(&related_buf[64 + 64 * i], related_item, strlen(related_item) + 1);
        }
    }
    ret = watch_mem_write(0, len, related_buf, area);
    if (ret != 0) {
        printf("watch mem werr %x\n", ret);
    } else {
        printf("watch mem succ\n");
    }

    watch_mem_close();

    free(related_buf);

    return ret;
}



static int watch_bgp_related_init()
{
    static u8 flag = 0;
    int ret = 0;
    u8 *related_buf;
    u32 total_relate_items = sizeof(watch_bgp_related) / sizeof(watch_bgp_related[0]);
    u32 len;
    char *related_item;
    u32 area = WATCH_MEM_BGP;
    u32 i, j;

    if (flag == 0) {
        watch_mem_new(area);
        flag = 1;
    }

    if (watch_bgp_items == 0) {
        return -1;
    }

    len = 64 * (total_relate_items + 1);
    related_buf = zalloc(len);
    if (related_buf == NULL) {
        return -1;
    }

    if (watch_mem_open() == NULL) {
        free(related_buf);
        return -1;
    }

    ret = watch_mem_read(0, len, related_buf, area);
    if (ret != 0) {
        free(related_buf);
        return -1;
    }

    for (i = 0; i < total_relate_items; i++) {
        watch_bgp_related[i] = NULL;
        related_item = &related_buf[64 + 64 * i];
        if (related_item[0]) {
            printf("related item : %s, %d, %d\n", related_item, strlen(related_item), i);
            for (j = 0; j < watch_bgp_items; j++) {
                if (strncmp(related_item, watch_bgp[j], strlen(watch_bgp[j])) == 0) {
                    watch_bgp_related[i] = 	watch_bgp[j];
                    printf("find bgp related %d\n", j);
                    break;
                }
            }
        }
    }

    watch_mem_close();

    free(related_buf);

    return 0;
}

#endif



int ui_watch_poweron_update_check()
{
#if(CONFIG_UI_STYLE == STYLE_JL_WTACH)
    //如果上次表盘升级异常，直接进入表盘升级模式等待升级完成
    //加入新标志位的判断，用于升级过程中断电后开机进入升级模式的情况
    /* if (smartbox_eflash_flag_get() != 0 || */
    /* smartbox_eflash_update_flag_get()) { */
    if (0) {
        printf("\n\ngoto watch update mode\n\n");

        //上电后，发现上一次表盘升级异常,先初始化升级需要的蓝牙相关部分,
        //再跳转到升级模式

        watch_update_over = 2;
        /* app_rcsp_task_prepare(0, SMARTBOX_TASK_ACTION_WATCH_TRANSFER, 0); */
        /* app_task_switch_to(APP_WATCH_UPDATE_TASK); */
        return -1;
        /* while (watch_update_over == 2) { */
        /* os_time_dly(3); */
        /* } */
        /* watch_update_over = 0; */
    }
#endif
    return 0;
}


int ui_upgrade_file_check_valid()
{
#if UI_UPGRADE_RES_ENABLE
    //简单实现
    //假设升级界面必须存在，调用了该接口证明资源不完整
    //需要进行升级
    /* smartbox_eflash_flag_set(1);//这个标志的清理需要注意 */
    return 0;
#endif
    return -ENOENT;
}

int watch_set_init()
{
#if UI_WATCH_RES_ENABLE
    u32 i, j;
    u32 len;
    u32 file_num;
    char *fname_buf;
    char *suffix = ".sty";
    u8 root_path_len = strlen(RES_PATH);
    u8 suffix_len = strlen(suffix);
    u8 fname_len;
    char *fname;


    for (i = 0; i < WATCH_ITEMS_LIMIT; i++) {
        if (watch_res[i] != NULL) {
            free(watch_res[i]);
            watch_res[i] = NULL;
        }
    }
    watch_style = 0;
    watch_items = 0;


    for (i = 0; i < BGP_ITEMS_LIMIT; i++) {
        if (watch_bgp[i]) {
            free(watch_bgp[i]);
            watch_bgp[i] = NULL;
        }
    }
    watch_bgp_items = 0;

#if defined(TCFG_VIRFAT_FLASH_ENABLE) && TCFG_VIRFAT_FLASH_ENABLE
    virfat_flash_get_dirinfo(NULL, &file_num);
#else
    file_num = 1;
#endif

    fname_buf = zalloc(file_num * 12);
    /* ASSERT((fname_buf != NULL), "fname_buf zalloc faile\n"); */
    if (fname_buf == NULL) {
        printf("fname_buf zalloc faile %d\n", file_num);
        /* while (1); */
        return -1;
    }

#if defined(TCFG_VIRFAT_FLASH_ENABLE) && TCFG_VIRFAT_FLASH_ENABLE
    virfat_flash_get_dirinfo(fname_buf, &file_num);
#else
    strcpy(fname_buf, "WATCH       ");
#endif

    printf("fnum %d\n", file_num);
    /* ASSERT((file_num < WATCH_ITEMS_LIMIT), "file num overflow %d\n", file_num); */
    /* if (file_num >= WATCH_ITEMS_LIMIT) { */
    if (file_num >= BGP_ITEMS_LIMIT) {
        printf("file num overflow %d\n", file_num);
        free(fname_buf);
        /* while (1); */
        return -1;
    }

    for (i = 0; i < file_num; i++) {
        fname = &fname_buf[i * 12];
        for (j = 0; j < 12; j++) {
            /* printf("j %x, %x\n", j, fname[j]); */
            if (fname[j] == ' ') {
                fname[j] = '\0';
                break;
            }
        }
        /* ASSERT((j != 12), "\n\n\n\nfname overflow\n\n\n\n\n"); */
        if (j == 12) {
            printf("\n\n\n\nfname overflow\n\n\n\n\n");
            free(fname_buf);
            /* while (1); */
            return -1;
        }
        fname_len = strlen(fname);
        ASCII_ToLower(fname, fname_len);
        //printf("fname %d, %d, %s\n", j, fname_len, fname);
        /* if (strncmp(fname, "watch", strlen("watch")) != 0) { */
        /* continue; */
        /* } */

        if (strncmp(fname, "watch", strlen("watch")) == 0) {
            len = root_path_len + fname_len + 1 + fname_len + suffix_len;
            watch_res[watch_items] = malloc(len + 1);
            /* ASSERT((watch_res[watch_items] != NULL), "\n\nmalloc watch list err\n\n"); */
            if (watch_res[watch_items] == NULL) {
                printf("\n\nmalloc watch list err\n\n");
                free(fname_buf);
                /* while (1); */
                return -1;
            }
            if (!strcmp(RES_PATH, EXTERN_PATH)) {
                strcpy(watch_res[watch_items], RES_PATH);
                strcpy(&watch_res[watch_items][root_path_len], fname);
                watch_res[watch_items][root_path_len + fname_len] = '/';
                strcpy(&watch_res[watch_items][root_path_len + fname_len + 1], fname);
                strcpy(&watch_res[watch_items][root_path_len + fname_len + 1 + fname_len], suffix);
                watch_res[watch_items][len] = '\0';
            } else {
                strcpy(watch_res[watch_items], RES_PATH);
                strcpy(&watch_res[watch_items][root_path_len], fname);
                strcpy(&watch_res[watch_items][root_path_len + fname_len], suffix);
                watch_res[watch_items][len] = '\0';
            }

            printf("watch list : %s, %d, %d\n", watch_res[watch_items], watch_items, len);

            watch_items++;

        } else if (strncmp(fname, "bgp_w", strlen("bgp_w")) == 0) {
            watch_bgp_add(fname);
        }

    }


    free(fname_buf);

    if (watch_bgp_related_init() != 0) {
        printf("bgp_related_init fail\n");
    } else {
        printf("bgp_related_init succ\n");
    }

    if ((sizeof(watch_style) != syscfg_read(VM_WATCH_SELECT, &watch_style, sizeof(watch_style))) || (watch_style >= watch_items)) {
        watch_style = 0;
    }


    /* if (wmem_test_flag == 1) { */
    /* watch_bgp_set_related("bgp_w0", 0); */
    /* watch_bgp_set_related("bgp_w2", 1); */
    /* watch_bgp_set_related("bgp_w1", 2); */
    /* watch_bgp_set_related("bgp_w0", 3); */
    /* watch_bgp_set_related("bgp_w1", 5); */
    /* } */


    /* watch_bgp_add("dial0.bin"); */
    /* watch_bgp_add("dial1.bin"); */
    /* watch_bgp_add("dial2.bin"); */


    /* watch_bgp_del("dial1.bin"); */

    /* watch_bgp_set_related("dial0.bin", 0); */
    /* watch_bgp_set_related("dial2.bin", 1); */
    /* watch_bgp_set_related("dial1.bin", 2); */
    /* watch_bgp_set_related("dial2.bin", 3); */
    /* watch_bgp_set_related("dial1.bin", 4); */
    /* watch_bgp_set_related("dial0.bin", 5); */

#endif

    return 0;
}


static OS_SEM g_watch_bgp_sem;

int ui_file_check_valid()
{
#if UI_WATCH_RES_ENABLE
    int ret;
    printf("open_resouece_file...\n");
    int i = 0;
    RESFILE *file = NULL;
    char *sty_suffix = ".sty";
    char *res_suffix = ".res";
    char *str_suffix = ".str";
    char tmp_name[100];
    /* u32 list_len = sizeof(WATCH_STY_CHECK_LIST) / sizeof(WATCH_STY_CHECK_LIST[0]); */
    u32 list_len;
    u32 tmp_strlen;
    u32 suffix_len;


    os_sem_create(&g_watch_bgp_sem, 0);
    //如果上次表盘升级异常，直接进入表盘升级模式等待升级完成
    /* if (smartbox_eflash_flag_get() != 0 || */
    /*     smartbox_eflash_update_flag_get()) { */
    if (smartbox_eflash_update_flag_get() != 0 ||
        smartbox_eflash_resume_opt()) {
        /* if (0) { */
        printf("\n\nneed goto watch update mode\n\n");
        return -EINVAL;
    } else {
        ret = watch_set_init();
        if (ret != 0) {
            ASSERT(0);
            return -EINVAL;
        }
    }

    list_len = watch_items;

    for (i = 0; i < list_len; i++) {
        if (file) {
            res_fclose(file);
            file = NULL;
        }
        /* file = res_fopen(WATCH_STY_CHECK_LIST[i], "r"); */
        printf("watch_res[%d] : %s\n", i, watch_res[i]);
        file = res_fopen(watch_res[i], "r");
        if (!file) {
            ASSERT(0);
            return -ENOENT;
        }
    }


    suffix_len = strlen(sty_suffix);
    close_resfile();
    for (i = 0; i < list_len; i++) {

        memset(tmp_name, 0, sizeof(tmp_name));
        /* tmp_strlen = strlen(WATCH_STY_CHECK_LIST[i]); */
        /* strcpy(tmp_name, WATCH_STY_CHECK_LIST[i]); */
        tmp_strlen = strlen(watch_res[i]);
        strcpy(tmp_name, watch_res[i]);
        strcpy(&tmp_name[tmp_strlen - suffix_len], res_suffix);
        tmp_name[tmp_strlen - suffix_len + strlen(res_suffix)] = '\0';

        /* ret = open_resfile(WATCH_RES_CHECK_LIST[i]); */
        printf("open_resfile tmp_name : %s\n", tmp_name);
        ret = open_resfile(tmp_name);
        if (ret) {
            ASSERT(0);
            return -EINVAL;
        }
        close_resfile();
    }


    close_str_file();
    for (i = 0; i < list_len; i++) {

        memset(tmp_name, 0, sizeof(tmp_name));
        /* tmp_strlen = strlen(WATCH_STY_CHECK_LIST[i]); */
        /* strcpy(tmp_name, WATCH_STY_CHECK_LIST[i]); */
        tmp_strlen = strlen(watch_res[i]);
        strcpy(tmp_name, watch_res[i]);
        strcpy(&tmp_name[tmp_strlen - suffix_len], str_suffix);
        tmp_name[tmp_strlen - suffix_len + strlen(str_suffix)] = '\0';

        /* ret = open_str_file(WATCH_STR_CHECK_LIST[i]); */
        ret = open_resfile(tmp_name);
        if (ret) {
            ASSERT(0);
            return -EINVAL;
        }
        close_str_file();
    }

#endif
    return 0;
}


