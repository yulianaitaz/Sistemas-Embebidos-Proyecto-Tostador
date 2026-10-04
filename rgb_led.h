#ifndef RGB_LED_H
#define RGB_LED_H


#include "esp_err.h"


/* bit0 = rojo, bit1 = verde, bit2 = azul */
typedef enum {
    RGB_OFF = 0, RGB_ROJO = 1, RGB_VERDE = 2, RGB_AMARILLO = 3,
    RGB_AZUL = 4, RGB_MAGENTA = 5, RGB_CIAN = 6, RGB_BLANCO = 7,
} rgb_color_t;


esp_err_t rgb_led_init(void);
void rgb_led_set(rgb_color_t c);


#endif
