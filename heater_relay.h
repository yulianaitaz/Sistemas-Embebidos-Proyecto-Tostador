#ifndef HEATER_RELAY_H
#define HEATER_RELAY_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t rele_init(void);          /* arranca APAGADO */
void rele_set(bool encendido);
bool rele_esta_encendido(void);

#endif