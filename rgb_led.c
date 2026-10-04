#include "driver/gpio.h"
#include "config_pines.h"
#include "rgb_led.h"


static inline int nivel(int on) { return RGB_COMMON_ANODE ? !on : on; }


void rgb_led_set(rgb_color_t c)
{
    gpio_set_level(PIN_RGB_R, nivel(c & 1));
    gpio_set_level(PIN_RGB_G, nivel((c >> 1) & 1));
    gpio_set_level(PIN_RGB_B, nivel((c >> 2) & 1));
}


esp_err_t rgb_led_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_RGB_R) | (1ULL << PIN_RGB_G) | (1ULL << PIN_RGB_B),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) return err;
    rgb_led_set(RGB_OFF);
    return ESP_OK;
}
