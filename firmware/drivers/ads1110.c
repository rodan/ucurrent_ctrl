
//  library for ADS1110 - 16bit analog to digital converter
//  author:          Petre Rodan <2b4eda@subdimension.ro>
//  available from:  https://github.com/rodan/
//  license:         GNU GPLv3

#include "proj.h"
#include "ads1110.h"
#include "serial_bitbang.h"

#ifdef HARDWARE_I2C
    #include "drivers/i2c.h"
#endif


// returns  0 if all is fine
//          I2C err levels if sensor is not properly hooked up

uint8_t ads1110_read(const uint8_t slave_addr, struct ads1110 *adc)
{
    uint8_t val[3] = { 0, 0, 0 };
    //uint8_t rdy;

#ifdef HARDWARE_I2C
    i2c_package_t pkg;
    pkg.slave_addr = slave_addr;
    pkg.addr[0] = 0;
    pkg.addr_len = 0;
    pkg.data = val;
    pkg.data_len = 3;
    pkg.read = 1;
    //pkg.options = I2C_READ | I2C_LAST_NAK;

    i2c_transfer_start(&pkg, NULL);
#else
    uint8_t rv;
    rv = i2cm_rxfrom(slave_addr, val, 3);

    if (rv != I2C_ACK) {
        return rv;
    }
#endif
    adc->conv_raw = ( val[0] << 8 ) + val[1];
    adc->config = val[2];
    //rdy = (val[2] & 0x80) >> 7;
    return EXIT_SUCCESS;
}

void ads1110_convert(struct ads1110 *adc)
{
    uint8_t bits;
    uint8_t PGA; // gain setting (0-3)
    int32_t tmp;

    bits = (adc->config & 0xc) >> 2; // DR aka data rate
    if (bits == 0) {
        bits = 12;
    } else {
        bits += 13;
    }
    PGA = adc->config & 0x3;

    tmp = ((int32_t) adc->conv_raw * 20480) >> (bits - 1 + PGA);
    adc->conv = tmp;
}

// set the configuration register
uint8_t ads1110_config(const uint8_t slave_addr, const uint8_t val)
{

#ifdef HARDWARE_I2C
    uint8_t data = val;

    pkg.slave_addr = slave_addr;
    pkg.addr[0] = 0;
    pkg.addr_len = 0;
    pkg.data = &data;
    pkg.data_len = 1;
    pkg.read = 0;

    i2c_transfer_start(&pkg, NULL);
#else
    uint8_t rv;
    rv = i2cm_txbyte(slave_addr, val);
    return rv;
#endif

    return EXIT_SUCCESS;
}

