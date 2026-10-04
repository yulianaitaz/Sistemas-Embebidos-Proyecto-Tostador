#include "i2c_bus.h"
#include "config_pines.h"

static i2c_master_bus_handle_t s_bus = NULL;


esp_err_t i2c_bus_init(void)
{
    i2c_master_bus_config_t cfg = {
        .i2c_port   = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_bus);
}


i2c_master_bus_handle_t i2c_bus_get_handle(void)
{
    return s_bus;
}
