#include "app_config.h"
#include "big.h"
#include "wireless_dev_manager.h"

#if LEA_BIG_CTRLER_TX_EN

static int big_tx_init(void *priv)
{
    return big_tx_ops.init();
}

static int big_tx_uninit(void *priv)
{
    return big_tx_ops.uninit();
}

static int big_tx_open(void *priv)
{
    return big_tx_ops.open((big_parameter_t *)priv);
}

static int big_tx_close(void *priv)
{
    uint8_t big_hdl = *((uint8_t *)priv);

    return big_tx_ops.close(big_hdl);
}

static int big_tx_ioctrl(int op, ...)
{
    va_list argptr;
    int res = BIG_OPST_SUCC;

    va_start(argptr, op);

    switch (op) {
    case WIRELESS_DEV_OP_SEND_PACKET:
        res = big_tx_ops.send_packet((const uint8_t *)va_arg(argptr, int), va_arg(argptr, int), (big_stream_param_t *)va_arg(argptr, int));
        break;

    case WIRELESS_DEV_OP_GET_BLE_CLK:
        big_tx_ops.get_ble_clk((uint32_t *)va_arg(argptr, uint32_t));
        break;

    case WIRELESS_DEV_OP_GET_TX_SYNC:
        res = big_tx_ops.get_tx_sync(va_arg(argptr, uint16_t));
        break;

    case WIRELESS_DEV_OP_SET_SYNC:
        big_tx_ops.sync_set(va_arg(argptr, uint16_t), va_arg(argptr, uint8_t));
        break;

    case WIRELESS_DEV_OP_GET_SYNC:
        big_tx_ops.sync_get(va_arg(argptr, uint16_t), (uint16_t *)va_arg(argptr, int), (uint32_t *)va_arg(argptr, int), (uint32_t *)va_arg(argptr, int));
        break;

    case WIRELESS_DEV_OP_STATUS_SYNC:
        if (big_tx_ops.padv_set_data) {
            big_tx_ops.padv_set_data((uint8_t *)va_arg(argptr, int), va_arg(argptr, int), (void *)va_arg(argptr, int));
        }
        break;

    case WIRELESS_DEV_OP_ENTER_PAIR:
        big_tx_ops.enter_pair(va_arg(argptr, uint8_t), (pair_callback_t *)va_arg(argptr, int));
        break;

    case WIRELESS_DEV_OP_EXIT_PAIR:
        big_tx_ops.exit_pair(va_arg(argptr, uint8_t));
        break;

    default :
        break;
    }

    va_end(argptr);

    return res;
}

REGISTER_WIRELESS_DEV(big_tx_op) = {
    .name           = "big_tx",
    .init           = big_tx_init,
    .uninit         = big_tx_uninit,
    .open           = big_tx_open,
    .close          = big_tx_close,
    .ioctrl         = big_tx_ioctrl,
};
#endif /* LEA_BIG_CTRLER_TX_EN */


#if LEA_BIG_CTRLER_RX_EN

static int big_rx_init(void *priv)
{
    return big_rx_ops.init();
}

static int big_rx_uninit(void *priv)
{
    return big_rx_ops.uninit();
}

static int big_rx_open(void *priv)
{
    return big_rx_ops.open((big_parameter_t *)priv);
}

static int big_rx_close(void *priv)
{
    uint8_t big_hdl = *((uint8_t *)priv);

    return big_rx_ops.close(big_hdl);
}

static int big_rx_ioctrl(int op, ...)
{
    va_list argptr;
    int res = BIG_OPST_SUCC;

    va_start(argptr, op);

    switch (op) {
    case WIRELESS_DEV_OP_SET_SYNC:
        big_rx_ops.sync_set(va_arg(argptr, uint16_t), va_arg(argptr, uint8_t));
        break;

    case WIRELESS_DEV_OP_GET_SYNC:
        big_rx_ops.sync_get(va_arg(argptr, uint16_t), (uint16_t *)va_arg(argptr, int), (uint32_t *)va_arg(argptr, int), (uint32_t *)va_arg(argptr, int));
        break;

    case WIRELESS_DEV_OP_ENTER_PAIR:
        big_rx_ops.enter_pair(va_arg(argptr, uint8_t), (pair_callback_t *)va_arg(argptr, int));
        break;

    case WIRELESS_DEV_OP_EXIT_PAIR:
        big_rx_ops.exit_pair(va_arg(argptr, uint8_t));
        break;

    default :
        break;
    }

    va_end(argptr);

    return res;
}

REGISTER_WIRELESS_DEV(big_rx_op) = {
    .name           = "big_rx",
    .init           = big_rx_init,
    .uninit         = big_rx_uninit,
    .open           = big_rx_open,
    .close          = big_rx_close,
    .ioctrl         = big_rx_ioctrl,
};
#endif /* LEA_BIG_CTRLER_RX_EN */

