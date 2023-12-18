#include "app_config.h"
#include "asm/clock.h"
#include "system/timer.h"
#include "asm/uart_dev.h"
#include "atk1218_bd.h"
#include "asm/cpu.h"
#include "generic/gpio.h"

#if defined(TCFG_GPS_DEV_ENABLE) && TCFG_GPS_DEV_ENABLE

#undef LOG_TAG_CONST
#define LOG_TAG     "[gps]"
#define LOG_ERROR_ENABLE
#define LOG_INFO_ENABLE
#include "debug.h"


#define UART_MAX_RECV_LEN		1024		//最大接收缓存字节数
u8 uart_rx_buf[UART_MAX_RECV_LEN] = {0}; 	//接收缓冲,最大UART_MAX_RECV_LEN个字节.
uart_bus_t *gps_uart = NULL;
volatile u8 uart_rx_sta = 0;
u16 rx_len = 0;

u8 uart_rx_gpsdata[UART_MAX_RECV_LEN] = {0};
const u32 _BAUD_id[9] = {4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600}; //模块支持波特率数组

//从buf里面得到第cx个逗号所在的位置
//返回值:0~0XFE,代表逗号所在位置的偏移.
//       0XFF,代表不存在第cx个逗号
static u8 nmea_comma_pos(u8 *buf, u8 cx)
{
    u8 *p = buf;
    while (cx) {
        if (*buf == '*' || *buf < ' ' || *buf > 'z') {
            return 0XFF;    //遇到'*'或者非法字符,则不存在第cx个逗号
        }
        if (*buf == ',') {
            cx--;
        }
        buf++;
    }
    return buf - p;
}
//m^n函数
//返回值:m^n次方.
static u32 nmea_pow(u8 m, u8 n)
{
    u32 result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}
//str转换为数字,以','或者'*'结束
//buf:数字存储区
//dx:小数点位数,返回给调用函数
//返回值:转换后的数值
static int nmea_str2num(u8 *buf, u8 *dx)
{
    u8 *p = buf;
    u32 ires = 0, fres = 0;
    u8 ilen = 0, flen = 0, i;
    u8 mask = 0;
    int res;
    while (1) {
        if (*p == '-') {    //负数
            mask |= 0X02;
            p++;
        }
        if (*p == ',' || (*p == '*')) {  //结束
            break;
        }
        if (*p == '.') {
            mask |= 0X01;
            p++;
        } else if (*p > '9' || (*p < '0')) { //非法字符
            ilen = 0;
            flen = 0;
            break;
        }
        if (mask & 0X01) {
            flen++;
        } else {
            ilen++;
        }
        p++;
    }
    if (mask & 0X02) {
        buf++;
    }
    for (i = 0; i < ilen; i++) { //整数部分数据
        ires += nmea_pow(10, ilen - 1 - i) * (buf[i] - '0');
    }
    if (flen > 5) {
        flen = 5;
    }
    *dx = flen;         //小数点位数
    for (i = 0; i < flen; i++) { //小数部分数据
        fres += nmea_pow(10, flen - 1 - i) * (buf[ilen + 1 + i] - '0');
    }
    res = ires * nmea_pow(10, flen) + fres;
    if (mask & 0X02) {
        res = -res;
    }
    return res;
}
//分析GPGSV信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void nmea_gpgsv_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    u8 *p, *p1, dx;
    u8 len, i, j, slx = 0;
    u8 posx;
    p = buf;
    p1 = (u8 *)strstr((const char *)p, "$GPGSV");
    if (p1 == NULL) {
        log_info("no GPGSV!\n");
        return;
    } else {
        /* log_info("receive GPGSV!\n"); */
    }
    len = p1[7] - '0';                          //GPGSV的条数
    posx = nmea_comma_pos(p1, 3);               //可见卫星总数
    if (posx != 0XFF) {
        gpsx->svnum = nmea_str2num(p1 + posx, &dx);
    }
    for (i = 0; i < len; i++) {
        p1 = (u8 *)strstr((const char *)p, "$GPGSV");
        for (j = 0; j < 4; j++) {
            posx = nmea_comma_pos(p1, 4 + j * 4);
            if (posx != 0XFF) {
                gpsx->slmsg[slx].num = nmea_str2num(p1 + posx, &dx);    //卫星编号
            } else {
                break;
            }
            posx = nmea_comma_pos(p1, 5 + j * 4);
            if (posx != 0XFF) {
                gpsx->slmsg[slx].eledeg = nmea_str2num(p1 + posx, &dx); //卫星仰角
            } else {
                break;
            }
            posx = nmea_comma_pos(p1, 6 + j * 4);
            if (posx != 0XFF) {
                gpsx->slmsg[slx].azideg = nmea_str2num(p1 + posx, &dx); //卫星方位角
            } else {
                break;
            }
            posx = nmea_comma_pos(p1, 7 + j * 4);
            if (posx != 0XFF) {
                gpsx->slmsg[slx].sn = nmea_str2num(p1 + posx, &dx);    //卫星信噪比
            } else {
                break;
            }
            slx++;
        }
        p = p1 + 1; //切换到下一个GPGSV信息
    }
}
//分析BDGSV信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void nmea_bdgsv_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    u8 *p, *p1, dx;
    u8 len, i, j, slx = 0;
    u8 posx;
    p = buf;
    p1 = (u8 *)strstr((const char *)p, "$BDGSV");
    if (p1 == NULL) {
        log_info("no BDGSV!\n");
        return;
    } else {
        /* log_info("receive BDGSV!\n"); */
    }
    len = p1[7] - '0';                          //BDGSV的条数
    posx = nmea_comma_pos(p1, 3);               //可见北斗卫星总数
    if (posx != 0XFF) {
        gpsx->beidou_svnum = nmea_str2num(p1 + posx, &dx);
    }
    for (i = 0; i < len; i++) {
        p1 = (u8 *)strstr((const char *)p, "$BDGSV");
        for (j = 0; j < 4; j++) {
            posx = nmea_comma_pos(p1, 4 + j * 4);
            if (posx != 0XFF) {
                gpsx->beidou_slmsg[slx].num = nmea_str2num(p1 + posx, &dx);    //卫星编号
            } else {
                break;
            }
            posx = nmea_comma_pos(p1, 5 + j * 4);
            if (posx != 0XFF) {
                gpsx->beidou_slmsg[slx].eledeg = nmea_str2num(p1 + posx, &dx); //卫星仰角
            } else {
                break;
            }
            posx = nmea_comma_pos(p1, 6 + j * 4);
            if (posx != 0XFF) {
                gpsx->beidou_slmsg[slx].azideg = nmea_str2num(p1 + posx, &dx);    //卫星方位角
            } else {
                break;
            }
            posx = nmea_comma_pos(p1, 7 + j * 4);
            if (posx != 0XFF) {
                gpsx->beidou_slmsg[slx].sn = nmea_str2num(p1 + posx, &dx);    //卫星信噪比
            } else {
                break;
            }
            slx++;
        }
        p = p1 + 1; //切换到下一个BDGSV信息
    }
}
//分析GNGGA信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void nmea_gngga_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    u8 *p1, dx;
    u8 posx;
    p1 = (u8 *)strstr((const char *)buf, "$GNGGA");
    if (p1 == NULL) {
        log_info("no GNGGA!\n");
        return;
    } else {
        /* log_info("receive GNGGA!\n"); */
    }
    posx = nmea_comma_pos(p1, 6);                           //GPS状态
    if (posx != 0XFF) {
        gpsx->gpssta = nmea_str2num(p1 + posx, &dx);
    }
    posx = nmea_comma_pos(p1, 7);                           //用于定位的卫星数
    if (posx != 0XFF) {
        gpsx->posslnum = nmea_str2num(p1 + posx, &dx);
    }
    posx = nmea_comma_pos(p1, 9);                           //海拔高度
    if (posx != 0XFF) {
        gpsx->altitude = nmea_str2num(p1 + posx, &dx);
    }
}
//分析GNGSA信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void nmea_gngsa_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    u8 *p1, dx;
    u8 posx;
    u8 i;
    p1 = (u8 *)strstr((const char *)buf, "$GNGSA");
    if (p1 == NULL) {
        log_info("no GNGSA!\n");
        return;
    } else {
        /* log_info("receive GNGSA!\n"); */
    }
    posx = nmea_comma_pos(p1, 2);                           //定位类型
    if (posx != 0XFF) {
        gpsx->fixmode = nmea_str2num(p1 + posx, &dx);
    }
    for (i = 0; i < 12; i++) {                              //定位卫星编号
        posx = nmea_comma_pos(p1, 3 + i);
        if (posx != 0XFF) {
            gpsx->possl[i] = nmea_str2num(p1 + posx, &dx);
        } else {
            break;
        }
    }
    posx = nmea_comma_pos(p1, 15);                          //PDOP位置精度因子
    if (posx != 0XFF) {
        gpsx->pdop = nmea_str2num(p1 + posx, &dx);
    }
    posx = nmea_comma_pos(p1, 16);                          //HDOP位置精度因子
    if (posx != 0XFF) {
        gpsx->hdop = nmea_str2num(p1 + posx, &dx);
    }
    posx = nmea_comma_pos(p1, 17);                          //VDOP位置精度因子
    if (posx != 0XFF) {
        gpsx->vdop = nmea_str2num(p1 + posx, &dx);
    }
}
//分析GNRMC信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void nmea_gnrmc_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    u8 *p1, dx;
    u8 posx;
    u32 temp;
    float rs;
    p1 = (u8 *)strstr((const char *)buf, "$GNRMC"); //"$GNRMC",经常有&和GNRMC分开的情况,故只判断GPRMC.
    if (p1 == NULL) {
        log_info("no GNRMC!\n");
        return;
    } else {
        /* log_info("receive GNRMC!\n"); */
    }
    posx = nmea_comma_pos(p1, 1);                           //UTC时间
    if (posx != 0XFF) {
        temp = nmea_str2num(p1 + posx, &dx) / nmea_pow(10, dx); //UTC时间,去掉ms
        gpsx->utc.hour = temp / 10000;
        gpsx->utc.min = (temp / 100) % 100;
        gpsx->utc.sec = temp % 100;
    }
    posx = nmea_comma_pos(p1, 3);                           //纬度
    if (posx != 0XFF) {
        temp = nmea_str2num(p1 + posx, &dx);
        gpsx->latitude = temp / nmea_pow(10, dx + 2); //得到°
        rs = temp % nmea_pow(10, dx + 2);       //得到'
        gpsx->latitude = gpsx->latitude * nmea_pow(10, 5) + (rs * nmea_pow(10, 5 - dx)) / 60; //转换为°
    }
    posx = nmea_comma_pos(p1, 4);                           //南纬还是北纬
    if (posx != 0XFF) {
        gpsx->nshemi = *(p1 + posx);
    }
    posx = nmea_comma_pos(p1, 5);                           //得到经度
    if (posx != 0XFF) {
        temp = nmea_str2num(p1 + posx, &dx);
        gpsx->longitude = temp / nmea_pow(10, dx + 2); //得到°
        rs = temp % nmea_pow(10, dx + 2);       //得到'
        gpsx->longitude = gpsx->longitude * nmea_pow(10, 5) + (rs * nmea_pow(10, 5 - dx)) / 60; //转换为°
    }
    posx = nmea_comma_pos(p1, 6);                           //东经还是西经
    if (posx != 0XFF) {
        gpsx->ewhemi = *(p1 + posx);
    }
    posx = nmea_comma_pos(p1, 9);                           //得到UTC日期
    if (posx != 0XFF) {
        temp = nmea_str2num(p1 + posx, &dx);                //得到UTC日期
        gpsx->utc.date = temp / 10000;
        gpsx->utc.month = (temp / 100) % 100;
        gpsx->utc.year = 2000 + temp % 100;
    }
}
//分析GNVTG信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void nmea_gnvtg_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    u8 *p1, dx;
    u8 posx;
    p1 = (u8 *)strstr((const char *)buf, "$GNVTG");
    if (p1 == NULL) {
        log_info("no GNVGT!\n");
        return;
    } else {
        /* log_info("receive GNVGT!\n"); */
    }
    posx = nmea_comma_pos(p1, 7);                   //得到地面速率
    if (posx != 0XFF) {
        gpsx->speed = nmea_str2num(p1 + posx, &dx);
        if (dx < 3) {
            gpsx->speed *= nmea_pow(10, 3 - dx);    //确保扩大1000倍
        }
    }
}
//提取NMEA-0183信息
//gpsx:nmea信息结构体
//buf:接收到的GPS数据缓冲区首地址
static void gps_analysis(nmea_msg_t *gpsx, u8 *buf)
{
    nmea_gpgsv_analysis(gpsx, buf); //GPGSV解析
    nmea_bdgsv_analysis(gpsx, buf); //BDGSV解析
    nmea_gngga_analysis(gpsx, buf); //GNGGA解析
    nmea_gngsa_analysis(gpsx, buf); //GPNSA解析
    nmea_gnrmc_analysis(gpsx, buf); //GPNMC解析
    nmea_gnvtg_analysis(gpsx, buf); //GPNTG解析
}


/////////////////////////////// 配置代码////////////////////////////////
//检查CFG配置执行情况
//返回值:0,ACK成功
//       1,接收超时错误
//       2,没有找到同步字符
//       3,接收到NACK应答
//       4,数据错位
static u8 skytra_cfg_ack_check(void)
{
    u16 i, time = 2;
    u8 rval = 0;
    u32 time_out = 40;//等400ms.
    while (time--) {
        time_out = 40;
        rval = 0;
        while ((!uart_rx_sta) && (time_out--)) {
            os_time_dly(1);
        }
        uart_rx_sta = 0;
        if (rx_len == 9) { //串口接收错位处理
            /* log_info_hexdump(uart_rx_gpsdata,9); */
            if (uart_rx_gpsdata[0] == 0x0a) {
                gps_uart->kfifo.buf_out++;
                gps_uart->kfifo.buf_in++;
            } else if (uart_rx_gpsdata[0] != 0xa0) {
                rval = 4;//数据错位
                break;
            }
        }
        for (i = 0; i < rx_len; i++) {
            if (uart_rx_gpsdata[i] == 0X83) {
                break;
            } else if (uart_rx_gpsdata[i] == 0X84) {
                rval = 3;
                break;
            }
        }
        if (i == rx_len) {
            rval = 2;    //没有找到同步字符
        }

        /* log_info(".....ack:%x....time:%d....len:%d...time_out:%d\n", rval, time, rx_len, time_out); */
        rx_len = 0;
        if (rval == 0) {
            break;
        }
    }
    return rval;
}
//发送一批数据给Ublox NEO-6M，这里通过串口2发送
//dbuf：数据缓存首地址
//len：要发送的字节数
static void skytra_send_date(u8 *dbuf, u16 len)
{
    gps_uart->write(dbuf, len);
}

//配置SkyTra_GPS/北斗模块波特率
//返回值:0,执行成功;其他,执行失败
static u8 skytra_cfg_prt(u32 baud_id)
{
    u16 wait_cnt = 30;
    skytra_baudrate_t *cfg_prt = (skytra_baudrate_t *)uart_rx_gpsdata;
    cfg_prt->sos = 0XA1A0;
    cfg_prt->pl = 0X0400;       //有效数据长度(小端模式)
    cfg_prt->id = 0X05;         //配置波特率的ID
    cfg_prt->com_port = 0X00;       //操作串口1
    cfg_prt->baud_id = baud_id;     ////波特率对应编号
    cfg_prt->attributes = 1;      //保存到SRAM&FLASH
    cfg_prt->cs = cfg_prt->id ^ cfg_prt->com_port ^ cfg_prt->baud_id ^ cfg_prt->attributes;
    cfg_prt->end = 0X0A0D;      //发送结束符(小端模式)
    skytra_send_date((u8 *)cfg_prt, sizeof(skytra_baudrate_t));
    uart_rx_sta = 0;
    /* log_info_hexdump((u8 *)uart_rx_gpsdata,sizeof(skytra_baudrate_t)); */
    /* while ((!uart_rx_sta) && wait_cnt--) { */
    /*     os_time_dly(1); */
    /* } */
    /* gps_uart->set_baud(_BAUD_id[baud_id]);  //重新初始化串口2 */
    return skytra_cfg_ack_check();
}
//配置SkyTra_GPS/北斗模块的时钟脉冲宽度
//width:脉冲宽度1~100000(us)
//返回值:0,发送成功;其他,发送失败.
static u8 skytra_cfg_pps(u32 width)
{
    u32 temp = width;
    skytra_pps_width_t *cfg_tp = (skytra_pps_width_t *)uart_rx_gpsdata;
    temp = (width >> 24) | ((width >> 8) & 0X0000FF00) | ((width << 8) & 0X00FF0000) | ((width << 24) & 0XFF000000); //小端模式
    cfg_tp->sos = 0XA1A0;
    cfg_tp->pl = 0X0700;
    cfg_tp->id = 0X65 ;
    cfg_tp->sub_id = 0X01;
    cfg_tp->width = temp;     //脉冲宽度,us
    cfg_tp->attributes = 0X01; //保存到SRAM&FLASH
    cfg_tp->cs = cfg_tp->id ^ cfg_tp->sub_id ^ (cfg_tp->width >> 24) ^ ((cfg_tp->width >> 16) & 0XFF) ^ ((cfg_tp->width >> 8) & 0XFF) ^ (cfg_tp->width & 0XFF) ^ cfg_tp->attributes;
    cfg_tp->end = 0X0A0D;
    skytra_send_date((u8 *)cfg_tp, sizeof(skytra_pps_width_t));
    uart_rx_sta = 0;
    return skytra_cfg_ack_check();
}
//配置SkyTraF8-BD的更新速率
//Frep:（取值范围:1,2,4,5,8,10,20）测量时间间隔，单位为Hz，最大不能大于50Hz
//返回值:0,发送成功;其他,发送失败.
static u8 skytra_cfg_rate(u8 Frep)
{
    skytra_posrate_t *cfg_rate = (skytra_posrate_t *)uart_rx_gpsdata;
    cfg_rate->sos = 0XA1A0;
    cfg_rate->pl = 0X0300;
    cfg_rate->id = 0X0E;
    cfg_rate->rate = Frep;
    cfg_rate->attributes = 0X01;
    cfg_rate->cs = cfg_rate->id ^ cfg_rate->rate ^ cfg_rate->attributes;
    cfg_rate->end = 0X0A0D;
    skytra_send_date((u8 *)cfg_rate, sizeof(skytra_posrate_t));
    uart_rx_sta = 0;
    return skytra_cfg_ack_check();
}
//配置SkyTra_GPS/北斗模块消息输出间隔
//返回值:0,执行成功;其他,执行失败
u8 skytra_cfg_message_interval()
{
    skytra_message_interval_t *cfg_msg = (skytra_message_interval_t *)uart_rx_gpsdata;
    cfg_msg->sos = 0XA1A0;
    cfg_msg->pl = 0X0F00;       //有效数据长度(小端模式)
    cfg_msg->id = 0X64;         //固定
    cfg_msg->sub_id = 0X02;     //固定
    cfg_msg->gga_interval = 0X01; //see skytra_message_interval_t结构体
    cfg_msg->gsa_interval = 0X01;
    cfg_msg->gsv_interval = 0X03;
    cfg_msg->gll_interval = 0X01;
    cfg_msg->rmc_interval = 0X01;
    cfg_msg->vtg_interval = 0X01;
    cfg_msg->zda_interval = 0X01;
    cfg_msg->gns_interval = 0X00;
    cfg_msg->gbs_interval = 0X00;
    cfg_msg->grs_interval = 0X00;
    cfg_msg->dtm_interval = 0X00;
    cfg_msg->gst_interval = 0X00;
    cfg_msg->attributes = 1;      //保存到SRAM&FLASH
    cfg_msg->cs = cfg_msg->id ^ cfg_msg->sub_id ^ cfg_msg->gga_interval ^ cfg_msg->gsa_interval ^ cfg_msg->gsv_interval ^ cfg_msg->gll_interval ^ cfg_msg->rmc_interval ^ cfg_msg->vtg_interval ^ cfg_msg->zda_interval ^ cfg_msg->gns_interval ^ cfg_msg->gbs_interval ^ cfg_msg->grs_interval ^ cfg_msg->dtm_interval ^ cfg_msg->gst_interval ^ cfg_msg->attributes;
    cfg_msg->end = 0X0A0D;
    skytra_send_date((u8 *)cfg_msg, sizeof(skytra_message_interval_t));
    uart_rx_sta = 0;
    return skytra_cfg_ack_check();
}
//配置SkyTra_GPS/北斗模块消息类型
//type: 00:no output; 01:NMEA message; 02:binary message
//返回值:0,执行成功;其他,执行失败
u8 skytra_cfg_message_type(u8 type)
{
    skytra_message_type_t *cfg_msg_type = (skytra_message_type_t *)uart_rx_gpsdata;
    cfg_msg_type->sos = 0XA1A0;
    cfg_msg_type->pl = 0X0300;       //有效数据长度(小端模式)
    cfg_msg_type->id = 0X09;
    cfg_msg_type->type = type;
    cfg_msg_type->attributes = 1;
    cfg_msg_type->cs = cfg_msg_type->id ^ cfg_msg_type->type ^ cfg_msg_type->attributes;
    cfg_msg_type->end = 0X0A0D;
    skytra_send_date((u8 *)cfg_msg_type, sizeof(skytra_message_type_t));
    uart_rx_sta = 0;
    return skytra_cfg_ack_check();
}







//通过判断接收连续2个字符之间的时间差不大于10ms来决定是不是一次连续的数据
static void gpsuart_isr_cbfun(void *utxx, u32 isr_state)
{
    uart_bus_t *utx = (uart_bus_t *)utxx;
    /* JL_UART_TypeDef *uartx = (JL_UART_TypeDef *)(utx->argv); */
    /* static u16 time_cnt=0; */
    if (isr_state == UT_TX) {

    } else {
        /* time_cnt++; */
        uart_rx_sta = 1;
        /* uartx->CON0 |= BIT(7); */
        /* u32 _len = uartx->HRXCNT; */
        rx_len = utx->read(uart_rx_gpsdata, sizeof(uart_rx_gpsdata) - 1, 100); //接收数据长度
        /* log_info("rx len:%d,time:%d",rx_len,time_cnt); */
    }
}

extern u32 uart_read_hrxcnt(uart_bus_t *ut);
void uart_gps_init()
{
    u8 baud_id = 0, gps_init_flg = 1, fail_cnt = 0;
    u32 temp_cnt = 0;
    struct uart_platform_data_t arg;
    arg.tx_pin = IO_PORTA_03; //IO_PORTA_02;
    arg.rx_pin = IO_PORTA_04; //IO_PORT_DP;
    arg.rx_cbuf = uart_rx_buf;
    arg.rx_cbuf_size = sizeof(uart_rx_buf);
    arg.frame_length = 1024;
    arg.rx_timeout = 5;//随波特率修改
    arg.isr_cbfun = gpsuart_isr_cbfun; //
    arg.is_9bit = 0;
    arg.baud = 4800;
    arg.argv = JL_UART2;
__restart_init:
    gps_uart = uart_dev_open(&arg);
    if (gps_uart == 0) {
        log_error("------gps uartx init fail!--------\n");
    } else {
        if (fail_cnt == 5) {
            log_error("*******gps init fail!*******!\n");
            return;
        }
        log_info("------gps uartx init ok!--------\n");
        gps_init_flg = skytra_cfg_rate(GPS_SET_FREP);
        os_time_dly(3);
        while (gps_init_flg != 0) {
            if (baud_id >= 9) {
                baud_id = 0;
                log_error("gps init fail1!\n");
                uart_dev_close(gps_uart);
                fail_cnt++;
                goto __restart_init;
                /* break; */
            }
            temp_cnt = uart_read_hrxcnt(gps_uart);
            gps_uart->kfifo.buf_in += temp_cnt;
            log_error("gps init --------hrxcnt:%d~~~~~~~baud:%d", temp_cnt, baud_id);
            gps_uart->set_baud(_BAUD_id[baud_id]);  //重新初始化串口2
            os_time_dly(3);
            gps_init_flg = skytra_cfg_prt(GPS_SET_BAUD);//设置波特率115200
            baud_id++;
        }
        gps_uart->set_baud(_BAUD_id[GPS_SET_BAUD]);  //重新初始化串口2
        os_time_dly(8);//配置完波特率后至少80ms才能配置其它参数
        if (baud_id <= 9) {
            if (skytra_cfg_pps(GPS_SET_PPS_WIDTH) == 0) {
                os_time_dly(8);
                gps_init_flg = skytra_cfg_rate(GPS_SET_FREP);
                if (gps_init_flg != 0) {
                    baud_id = 0;
                    log_error("gps init fail2!\n");
                    uart_dev_close(gps_uart);
                    fail_cnt++;
                    goto __restart_init;
                }
                log_info("flag:%d------gps init ok!\n", gps_init_flg);
                os_time_dly(5);
                skytra_cfg_message_type(1);
                os_time_dly(5);
                /* sys_s_hi_timer_add(NULL, get_gps_message_continoue, 1000);       //定时1s获取GPS数据 */
            } else {
                baud_id = 0;
                log_error("gps init fail3!\n");
                uart_dev_close(gps_uart);
                fail_cnt++;
                goto __restart_init;
            }
        }
    }
}

const s8 *fixmode_str[4] = {"fail", "fail", " 2D ", " 3D "};	//fix mode字符串
//打印GPS定位信息
static void gps_msg_show(nmea_msg_t *new_gps_data)
{
    u32 temp = 0;
    int temp1 = 0;
    temp = new_gps_data->longitude;
    log_info("longitude*100000:%d %1c   ", temp, new_gps_data->ewhemi);	//经度字符串
    temp = new_gps_data->latitude;
    log_info("latitude*100000:%d %1c   ", temp, new_gps_data->nshemi);	//纬度字符串
    temp1 = new_gps_data->altitude;
    log_info("altitude*10:%dm     ", temp1);		//高度字符串
    temp = new_gps_data->speed;
    log_info("speed*1000:%dkm/h", temp);		//速度字符串
    if (new_gps_data->fixmode <= 3) {		//定位状态
        log_info("fix mode:%s", fixmode_str[new_gps_data->fixmode]);
    }
    log_info("GPS+BD valid satellite:%02d", new_gps_data->posslnum);	 		//用于定位的GPS卫星数
    log_info("GPS visible satellite:%02d", new_gps_data->svnum % 100);	 		//可见GPS卫星数

    log_info("BD visible satellite:%02d", new_gps_data->beidou_svnum % 100);	 		//可见北斗卫星数

    log_info("UTC date:%04d/%02d/%02d   ", new_gps_data->utc.year, new_gps_data->utc.month, new_gps_data->utc.date);	//显示UTC日期
    log_info("UTC time:%02d:%02d:%02d   ", new_gps_data->utc.hour, new_gps_data->utc.min, new_gps_data->utc.sec);	//显示UTC时间
}

nmea_msg_t new_gps_data;
void get_gps_message_continoue()//模块初始化为以GPS_SET_FREP频率连续输出数据
//调用前确保gps模块是连续输出的. 运行skytra_cfg_message_type(1);配置模块为连续输出
{
    /* u16 len = 0; */
    u32 time_out = 10;//等100ms.
    while ((!uart_rx_sta) && (time_out--)) {
        os_time_dly(1);
    }
    uart_rx_sta = 0;
    if (time_out) {
        log_info("new data!");
    } else {
        log_info("old data!");
    }
    /* len = gps_uart->read(uart_rx_gpsdata, sizeof(uart_rx_gpsdata) - 1, 400); //接收数据长度 */
    /* log_info("rx data:\n%s\n",uart_rx_gpsdata); */
    gps_analysis(&new_gps_data, uart_rx_gpsdata);
    gps_msg_show(&new_gps_data);
}

void get_gps_message_single()//调用此函数触发模块输出
//俩次调用需间隔50ms
{
    uart_rx_sta = 0;
    if (skytra_cfg_message_type(1) == 0) {
        while (!uart_rx_sta);
        uart_rx_sta = 0;
        /* log_info("rx data:\n%s\n",uart_rx_gpsdata); */
        gps_analysis(&new_gps_data, uart_rx_gpsdata);
        gps_msg_show(&new_gps_data);
        os_time_dly(1);
        skytra_cfg_message_type(0);
        uart_rx_sta = 0;
    }
}

#if 0
void uart1_readgps_test()
{
    //当开启DMA接收时(给rx_cbuf结构体成员赋值会打开DMA接收)，个数rx_cbuf_size 必须设为2的n次方
    u8  write_buf[64] = "1bcde2bcde3bcde4bcde5bcde6bcde7bcde8bcde9bcde0bcde12345678901234";
    u8 read_data[800] = {0}, rx_cnt = 0;
    u32 i = 0, time_out = 0;
    struct uart_platform_data_t arg;

    arg.tx_pin = IO_PORTA_03; //IO_PORT_DM; //IO_PORTA_02;
    arg.rx_pin = IO_PORTA_04; //IO_PORT_DP;
    arg.rx_cbuf = uart_rx_buf;
    arg.rx_cbuf_size = sizeof(uart_rx_buf);
    arg.frame_length = 1024;
    arg.rx_timeout = 10;
    arg.isr_cbfun = NULL;//gpsuart_isr_cbfun; //
    arg.argv = JL_UART2;
    arg.is_9bit = 0;
    arg.baud = 230400;//改成和GPS模块波特率相同
    log_info("----uart test!----\n");
    /* if (uart_dev_close(&uart1) == 0) { */
    /* 	log_error("~~~~~~~~~uart1 close fail!~~~~~~~~~~\n"); */
    /* } */
    gps_uart = uart_dev_open(&arg);
    if (gps_uart == 0) {
        log_error("------uart init fail!--------\n");
    } else {
        log_info("------uart init ok!--------\n");
        //发送
        gps_uart->putbyte(write_buf[0]);
        gps_uart->putbyte('\n');
        gps_uart->write(write_buf, sizeof(write_buf));
        u8 temp1[] = "\nTwo serial ports are required!\n";
        printf("\nuart wait for time \n");
        gps_uart->write(temp1, sizeof(temp1));
        //接收
        log_info("\nuart wait for time !\n");
        time_out = 400;
        rx_cnt = 5;
        while (rx_cnt) {
            u32 lenlen = gps_uart->read(read_data, sizeof(read_data), time_out);
            if (lenlen) {
                log_info("get buf: %s", read_data);
                /* log_info_hexdump(read_data, lenlen); */
                for (i = 0; i < sizeof(read_data); i++) {
                    read_data[i] = 0;
                }
                rx_cnt--;
            } else {
                log_info("receive buf fail!\t");
                log_info("please send some characters!\n");
            }
        }
    }
}
#endif



#endif
