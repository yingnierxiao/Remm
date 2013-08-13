#ifndef I2C_H
#define I2C_H

#include <stdint.h>

int init_i2c();
int i2c_write(uint8_t dev_addr, uint8_t *data, uint8_t len);
int i2c_read(uint8_t dev_addr, uint8_t *data, uint8_t len);

#endif // I2C_H
