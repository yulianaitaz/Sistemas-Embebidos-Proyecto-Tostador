#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "esp_err.h"
#include "driver/i2c_master.h"

/* Crea el bus I2C fisico una sola vez. Llamar antes de ds3231_init()
 * y oled_display_init() */
esp_err_t i2c_bus_init(void);


i2c_master_bus_handle_t i2c_bus_get_handle(void);


#endif /* I2C_BUS_H */
