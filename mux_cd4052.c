#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_pines.h"
#include "mux_cd4052.h"
static int s_canal_actual = -1;


esp_err_t mux_cd4052_init(void)
{
        gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << MUX_SEL_A_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) return err;


    gpio_set_level(MUX_SEL_A_PIN, 0);   /* arranca en canal café */
    s_canal_actual = MUX_CANAL_CAFE;
    return ESP_OK;
}


void mux_select_canal(int canal)
{
    if (canal == s_canal_actual) return;
    gpio_set_level(MUX_SEL_A_PIN, canal ? 1 : 0);
    s_canal_actual = canal;
    vTaskDelay(pdMS_TO_TICKS(MUX_SETTLE_MS));
}
