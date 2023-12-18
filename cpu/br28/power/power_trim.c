#include "asm/power_interface.h"
#include "app_config.h"
#include "includes.h"
#include "stdlib.h"

__attribute__((noinline))
AT(.volatile_ram_code)
static u8 wiovdd_trim(u8 trim, u32 *adc_wiovdd_voltage)
{
    P3_VLVD_CON = 0;
    u32 wiovdd_lev = 7;
    u32 miovdd_lev = 0;
    P33_CON_SET(P3_ANA_CON5, 3, 3, wiovdd_lev);

    delay(215);//延时50us
    P33_CON_SET(P3_ANA_CON5, 0, 3, miovdd_lev);
    delay(430);

    if (trim) {
        for (; wiovdd_lev > 3; wiovdd_lev--) {
            P33_CON_SET(P3_ANA_CON5, 3, 3, wiovdd_lev);
            delay(215);//延时50us
            P33_CON_SET(P3_ANA_CON5, 0, 3, miovdd_lev);
            delay(430);
            adc_wiovdd_voltage[wiovdd_lev] =  get_vddio_voltage();
        }
        adc_wiovdd_voltage[3] =  adc_wiovdd_voltage[4] - 200;
        adc_wiovdd_voltage[2] =  adc_wiovdd_voltage[3] - 200;
        adc_wiovdd_voltage[1] =  adc_wiovdd_voltage[2] - 200;
        adc_wiovdd_voltage[0] =  adc_wiovdd_voltage[1] - 200;
    }

    return 0;
}

__attribute__((noinline))
AT(.volatile_ram_code)
static u8 miovdd_trim(u8 trim, u32 *adc_miovdd_voltage)
{
    P3_VLVD_CON = 0;
    u32 wiovdd_lev = 0;
    u32 miovdd_lev = 7;
    P33_CON_SET(P3_ANA_CON5, 3, 3, wiovdd_lev);

    delay(215);//延时50us
    P33_CON_SET(P3_ANA_CON5, 0, 3, miovdd_lev);
    delay(430);

    if (trim) {
        for (; miovdd_lev > 3; miovdd_lev--) {
            P33_CON_SET(P3_ANA_CON5, 3, 3, wiovdd_lev);
            delay(215);//延时50us
            P33_CON_SET(P3_ANA_CON5, 0, 3, miovdd_lev);
            delay(430);
            adc_miovdd_voltage[miovdd_lev] =  get_vddio_voltage();
        }
        adc_miovdd_voltage[3] =  adc_miovdd_voltage[4] - 200;
        adc_miovdd_voltage[2] =  adc_miovdd_voltage[3] - 200;
        adc_miovdd_voltage[1] =  adc_miovdd_voltage[2] - 200;
        adc_miovdd_voltage[0] =  adc_miovdd_voltage[1] - 200;
    }

    return 0;
}

static u8 low_voltage = 0;
__attribute__((noinline))
AT(.volatile_ram_code)
u32 iovdd_trim(u8 trim, u8 *miovdd_lev, u8 *wiovdd_lev)
{

    const u32 m_iovdd_lev = P3_ANA_CON5 & 0xf;
    if (trim == 0) {
        return 1;
    }

    u32 check_vbat =  get_vdd_voltage(AD_CH_VBAT) * 4;
    printf("check_vbat = %d\n", check_vbat);
    if (check_vbat < 3700) {
        printf("%s    %d\n", __func__, __LINE__);
        low_voltage = 1;
        *miovdd_lev = m_iovdd_lev | BIT(7);
        *wiovdd_lev = m_iovdd_lev | BIT(7);
        return 0;
    }

    const u32 lvd_con = P3_VLVD_CON;
    P3_VLVD_CON &= ~BIT(2);
    delay(100);
    P3_VLVD_CON &= ~BIT(0); //关闭lvd

    u32 adc_wiovdd_voltage[8];
    u32 adc_miovdd_voltage[8];
    wiovdd_trim(trim, adc_wiovdd_voltage);
    miovdd_trim(trim, adc_miovdd_voltage);
    u32 miovdd_voltage = m_iovdd_lev * 200 + 2000;
    u32 min_diff = -1;
    u32 mlev = 0;

    for (int i = 0; i < 8; i++) {
        u32 diff = abs(miovdd_voltage - adc_miovdd_voltage[i]);
        if (diff < min_diff) {
            mlev = i;
            min_diff = diff;
        }
    }
    *miovdd_lev = mlev;

    u32 wlev = 0;
    min_diff = -1;
    for (int i = 0; i < 8; i++) {
        u32 diff = abs(adc_miovdd_voltage[mlev] - adc_wiovdd_voltage[i]);
        if (diff < min_diff) {
            wlev = i;
            min_diff = diff;
        }
    }

    *wiovdd_lev = wlev;
    SFR(P3_ANA_CON5, 0, 3, mlev);
    if (lvd_con & BIT(0)) {
        P3_VLVD_CON |= BIT(0);//en
        delay(3);
        P3_VLVD_CON |= BIT(2);//oe
    }

    for (u8 i = 0; i < 8; i++) {
        printf("adc_wiovdd_voltage[%d] = %d\n", i, adc_wiovdd_voltage[i]);
        printf("adc_miovdd_voltage[%d] = %d\n", i, adc_miovdd_voltage[i]);
    }
    return 0;
}

static u8 wvdd_trim(u8 trim)
{
    u8 wvdd_lev = 0;
    wvdd_lev = 0;
    int v = 0;
    u8 err = 0;

    if (trim) {

        v = get_wvdd_level_trim();
        if (v == 0) {
            //0.65v
            wvdd_lev = 0b0011;
        } else if (v == 1) {
            //0.7v
            wvdd_lev = 0b0100;
        } else if (v == 2) {
            //0.75v
            wvdd_lev = 0b0101;
        } else {
            //0.8v
            //wvdd_lev = 0b0110;
            WVDD_VOL_SEL(wvdd_lev);
            WVDD_LOAD_EN(1);
            WLDO06_EN(1);
            delay(2000);//1ms
            do {
                WVDD_VOL_SEL(wvdd_lev);
                delay(2000);//1ms * n
                v = get_vdd_voltage(AD_CH_WVDD);
                if (v > WVDD_VOL_TRIM) {
                    break;
                }
                wvdd_lev ++;
            } while (wvdd_lev < WVDD_LEVEL_MAX);
            WVDD_LOAD_EN(0);
            WLDO06_EN(0);

            u8 min = (WVDD_VOL_TRIM - WVDD_VOL_MIN) / WVDD_VOL_STEP - 2;
            u8 max = (WVDD_VOL_TRIM - WVDD_VOL_MIN) / WVDD_VOL_STEP + 2;
            if (!(wvdd_lev >= min && wvdd_lev <= max)) {
                wvdd_lev = WVDD_LEVEL_DEFAULT;
                err = 1;
            }
        }

        //update_wvdd_trim_level(wvdd_lev);
    } else {
        wvdd_lev = get_wvdd_trim_level();
    }

    printf("trim: %d, wvdd_lev: %d, v: %d, err: %d\n", trim, wvdd_lev, v, err);

    M2P_MESSAGE_WVDD = wvdd_lev;

    if (err) {
        return WVDD_LEVEL_ERR;
    }

    return wvdd_lev;
}

static u8 pvdd_trim(u8 trim)
{
    /* return 0; */
    u32 v = 0;
    u32 lev = 0xf;
    u32 pvdd_trim_vol = PVDD_VOL_TRIM;
    u8 err = 0;

#ifdef CONFIG_WATCH_CASE_ENABLE
    pvdd_trim_vol -= 100;
#endif

    if (trim) {
        for (; lev; lev--) {
            PVDD_LEVEL_HIGH_NOW(lev);
            delay(2000);//1ms
            v = get_vdd_voltage(AD_CH_PVDD);
            if (v < (pvdd_trim_vol + PVDD_VOL_STEP)) {
                break;
            }
        }

        if (v < pvdd_trim_vol) {
            if (lev < PVDD_LEVEL_MAX) {
                lev++;
            }
        }

        u8 min = (pvdd_trim_vol - PVDD_VOL_MIN) / PVDD_VOL_STEP - 2;
        u8 max = (pvdd_trim_vol - PVDD_VOL_MIN) / PVDD_VOL_STEP + 2;
        if (!(lev >= min && lev <= max)) {
            lev = PVDD_LEVEL_DEFAULT;
            err = 1;
        }

        //update_pvdd_trim_level(lev);
    } else {
        lev = get_pvdd_trim_level();
    }

#if (!PMU_NEW_FLOW)
    if (get_pvdd_dcdc_cfg()) {
        //外接dcdc，pvdd配置为3档
        lev = 3;
    }
#endif

    PVDD_LEVEL_HIGH_NOW(lev);
    delay(2000);
    PVDD_AUTO_PRD(0x3);
    PVDD_LEVEL_AUTO(0x1);
    PVDD_LEVEL_LOW(M2P_MESSAGE_WVDD);
    /* P33_CON_SET(P3_PVDD0_AUTO, 0, 8, (7 << 4) | (lev - PVDD_LOW_LEVEL)); */


#if PMU_NEW_FLOW
    if (get_pvdd_dcdc_cfg()) {
        power_set_pvdd_mode(PWR_PVDD_DCDC);
    }
#endif

    printf("trim: %d, pvdd_lev: %d %d, pvdd_lev_l: %d, err: %d\n", trim, lev, v, M2P_MESSAGE_WVDD, err);

    if (err) {
        return PVDD_LEVEL_ERR;
    }

    return lev;
}

void volatage_trim_init()
{
    /* trim wvdd */
    printf("lvd_con = 0x%x\n", P3_VLVD_CON);
    u8 trim = load_pmu_trim_value_from_vm();
    u8 wvdd_lev = 0;
    u8 pvdd_lev = 0;
    printf("trim = 0x%x\n", trim);

    wvdd_lev = wvdd_trim((trim & TRIM_WVDD) ? 1 : 0);
    pvdd_lev = pvdd_trim((trim & TRIM_PVDD) ? 1 : 0);

    u8 miovdd_lev = 0;
    u8 wiovdd_lev = 0;
    if (trim & TRIM_IOVDD) {
        u8 trim_succ = iovdd_trim(trim, &miovdd_lev, &wiovdd_lev);
    } else {
        miovdd_lev = get_miovdd_trim_level();
        wiovdd_lev = get_wiovdd_trim_level();

    }

    if (trim) {
        if ((wvdd_lev != WVDD_LEVEL_ERR) && (pvdd_lev != PVDD_LEVEL_ERR)) {
            store_pmu_trim_value_to_vm(wvdd_lev, pvdd_lev, miovdd_lev, wiovdd_lev);
        }

    }

    printf("miovdd_lev = %d\n", miovdd_lev);
    printf("wiovdd_lev = %d\n", wiovdd_lev);
}


