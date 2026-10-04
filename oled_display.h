#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H
#include "esp_err.h"

/* Agrega el SSD1306 al bus I2C (creado por i2c_bus_init()) y lo inicializa. Llamar despues de i2c_bus_init(). */
esp_err_t oled_display_init(void);


/* Tarea de baja prioridad: muestra la hora del RTC y la última
 * lectura de cafe y tambor, incluyendo "DESCONECTADA" si aplica. */
void vTaskOled(void *pvParameters);

#endif /* OLED_DISPLAY_H */
