#ifndef _OHR_I2C_DRIVER_H_
#define _OHR_I2C_DRIVER_H_
#include "system/includes.h"
#include "app_config.h"
/**@brief write one byte to i2c slave
 *
 * @details  write one byte to i2c slave
 *
 * @note     
 * @param[in]  i2cAddress    
 * @param[in]  reg_adr 
 * @param[in]  datum  
 * @retval    FAIL        I2C transfer fail
 *            SUCC        I2C transfer done
 */
unsigned char I2CDevice_WriteOneByte(unsigned char i2cAddress,unsigned char reg_adr,unsigned char datum);

/**@brief Read block of data from i2c.
 *
 * @details  read numbers of byte from i2c slave
 *
 * @note     
 * @param[in]  i2cAddress    
 * @param[in]  reg_adr 
 * @param[in]  buffer     pointer to put read data  
 * @param[in]  dat_len    length of bytes to read  
 * @retval    FAIL        I2C transfer fail
 *            SUCC        I2C transfer done
 */
unsigned char I2CDevice_Read(unsigned char i2cAddress,unsigned char reg_adr,unsigned char *buffer,unsigned char dat_len);

/**@brief  Write block of data to i2c.
 *
 * @details  write numbers of byte to i2c slave
 *
 * @note     
 * @param[in]  i2cAddress    
 * @param[in]  reg_adr 
 * @param[in]  buffer     pointer of data to write
 * @param[in]  dat_len    length of bytes to write  
 * @retval    FAIL        I2C transfer fail
 *            SUCC        I2C transfer done
 */
unsigned char I2CDevice_Write(unsigned char i2cAddress,unsigned char reg_adr,unsigned char const *buffer,unsigned char dat_len);

void OHR_I2C_init(void);
void OHR_I2C_uninit(void);

#endif


