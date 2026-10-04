#ifndef TERMOCUPLAS_H
#define TERMOCUPLAS_H
#include "esp_err.h"


/* Agrega el MAX6675 al bus SPI (creado en main.c) e inicializa el mux. */
esp_err_t termocuplas_init(void);


/* Tarea de mayor prioridad: cada 5 s lee cafe (siempre) y cada 10 s
 * lee tambien tambor, publicando cada lectura en la cola. */
void vTaskSensorTermocuplas(void *pvParameters);


#endif /* TERMOCUPLAS_H */
