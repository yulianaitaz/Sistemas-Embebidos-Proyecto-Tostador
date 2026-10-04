#ifndef ROAST_CONTROL_H
#define ROAST_CONTROL_H


#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "data_types.h"


typedef enum { TUESTE_CLARO = 0, TUESTE_MEDIO, TUESTE_OSCURO, TUESTE_N } tueste_t;


typedef enum {
    TIEMPO_DETENIDO = 0,
    TIEMPO_TOSTANDO,
    TIEMPO_VENTANA_DESCARGA,   /* ya paso el tiempo minimo */
    TIEMPO_EXCEDIDO,           /* paso el tiempo maximo */
    TIEMPO_ENFRIANDO,          /* fase de enfriamiento (valor 4) */
} fase_tiempo_t;

typedef enum {
    FAN_APAGADO = 0,
    FAN_CONTROL_TERMICO,       /* regula temperatura durante el tueste */
    FAN_ENFRIAMIENTO,          /* enfriamiento posterior al tueste */
} fan_modo_t;

typedef struct {
    tueste_t      tueste;
    const char   *nombre;
    float         temp_min, temp_max;
    uint32_t      t_min_s, t_max_s, elapsed_s;
    fase_tiempo_t fase;
    fan_modo_t    fan;
} roast_info_t;


esp_err_t   roast_control_init(void);
alerta_t    roast_evaluar(sensor_id_t id, float temp, estado_sensor_t est);
void        roast_get_info(roast_info_t *out);
const char *roast_nombre_tueste(uint8_t t);
void        vTaskRoast(void *pvParameters);

bool roast_toggle(void);            /* inicia/detiene (igual que el botón); devuelve true si quedó en marcha */
bool roast_set_tueste(int n);       /* 0..2; solo si está detenido */
bool roast_fan_test_toggle(void);   /* prueba manual del ventilador; solo con el tueste detenido */

#endif
