#ifndef DATA_TYPES_H
#define DATA_TYPES_H
#include <stdint.h>
#define DATETIME_LEN 20   /* "YYYY-MM-DD HH:MM:SS" + '\0' */


typedef enum {
    SENSOR_CAFE   = 0,
    SENSOR_TAMBOR = 1,
} sensor_id_t;


typedef enum {
    ESTADO_OK           = 0,
    ESTADO_DESCONECTADA = 1,
} estado_sensor_t;


/* Una muestra = una lectura de UNA termocupla + la hora del RTC en ese
 * instante. Aunque la termocupla falle, timestamp/datetime siempre se
 * llenan, así el registro nunca pierde la referencia de tiempo. */
typedef enum {
    ALERTA_NINGUNA = 0,
    ALERTA_CALENTANDO,      
    ALERTA_TEMP_BAJA,
    ALERTA_TEMP_ALTA,
    ALERTA_DESCONECTADA,
} alerta_t;


 typedef struct {
    uint64_t        timestamp_ms;
    char             datetime[DATETIME_LEN];
    sensor_id_t      sensor_id;
    float            temp_celsius;
    estado_sensor_t  estado;


    uint8_t         tueste;       /* 0=claro 1=medio 2=oscuro */
    alerta_t        alerta;
    uint32_t        t_tueste_s;   /* segundos desde que se pulsó START */
}datalog_entry_t;


#endif /* DATA_TYPES_H */
