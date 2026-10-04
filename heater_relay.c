#include "driver/gpio.h"
#include "config_pines.h"
#include "heater_relay.h"

static bool s_on = false;

static inline int nivel(bool on) { return RELE_ACTIVO_BAJO ? !on : on; }

esp_err_t rele_init(void)
{
    gpio_set_level(PIN_RELE, nivel(false));   /* nivel "apagado" antes de activar la salida */
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_RELE),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&cfg);
    if (err != ESP_OK) return err;
    gpio_set_level(PIN_RELE, nivel(false));
    s_on = false;
    return ESP_OK;
}

void rele_set(bool encendido)
{
    gpio_set_level(PIN_RELE, nivel(encendido));
    s_on = encendido;
}

bool rele_esta_encendido(void) { return s_on; }