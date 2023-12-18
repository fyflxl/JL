#ifndef __IMD_MCU_H__
#define __IMD_MCU_H__


#define IMD_PAP_PND_IE()	SFR(JL_IMD->IMDPAP_CON0, 29, 1, 1)
#define IMD_PAP_PND_DIS()	SFR(JL_IMD->IMDPAP_CON0, 29, 1, 0)


struct imd_mcu_config_def {
    int out_format;
    int right_shift_2bit;
    int dat_l2h_en;
    int wr_sel;
    int rd_sel;

    void (*cs_ctrl)(u8);	// cs脚控制函数
    void (*dc_ctrl)(u8);	// dc脚控制函数
    void (*dat_dir)(u8);	// dat dir数据方向
};



void imd_pap_init(struct imd_mcu_config_def *priv);

void imd_pap_tx_cmd(int cmd, int cmd_cnt);

void imd_pap_tx_dat(int para, int para_cnt);

void imd_pap_rx_dat(int para, int para_cnt);

void imd_pap_rx_buf(u8 *buf, u16 len);

void imd_pap_set_draw_area(int xstart, int xend, int ystart, int yend);

void imd_pap_write_cmd(u8 cmd, u8 *buf, u8 len);

void imd_pap_read_cmd(u8 cmd, u8 *buf, u8 len);

void imd_pap_draw(void);

int  imd_pap_isr();



#endif

