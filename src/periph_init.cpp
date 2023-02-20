#include "pinDef.h"
#include "driver/i2c.h"

void i2c_init()
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BMS_I2C_SDApin, // 0
        .scl_io_num = BMS_I2C_CLKpin, // 4
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
//       .master.clk_speed = 400000,   
//       .clk_flags = 0,      
    };
    conf.master.clk_speed = 400000;
    ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));
};

int periph_init()
{
    i2c_init();
    return 0;
};