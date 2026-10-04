#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "motor_l298n.h"
#include "web_server.h"
#include "sd_logger.h"

#include "oled_display.h"
#include "i2c_bus.h"
#include "config_pines.h"
#include "shared_resources.h"
#include "data_types.h"
#include "ds3231.h"
#include "roast_control.h"


static const char *TAG = "OLED";
static i2c_master_dev_handle_t s_dev;


#define OLED_W     128
#define OLED_H     64
#define OLED_PAGES (OLED_H / 8)


static uint8_t s_fb[OLED_W * OLED_PAGES];


/* Fuente 5x7 reducida: solo los caracteres que usa esta app
 * (dígitos, ':', '-', '.', ',', espacio y las letras de
 * "CAFE / TAMBOR / DESCONECTADA / OK"). Cada glifo son 5 columnas
 * de 8 bits (bit0 = fila superior). */
typedef struct { char c; uint8_t col[5]; } glifo_t;


static const glifo_t FUENTE[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {':', {0x00,0x36,0x36,0x00,0x00}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'.', {0x00,0x60,0x60,0x00,0x00}},
    {',', {0x00,0x50,0x30,0x00,0x00}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'A', {0x7E,0x11,0x11,0x11,0x7E}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'/', {0x20,0x10,0x08,0x04,0x02}},
    {'!', {0x00,0x00,0x5F,0x00,0x00}},


};
#define N_GLIFOS (sizeof(FUENTE) / sizeof(FUENTE[0]))


static const uint8_t *buscar_glifo(char c)
{
    for (size_t i = 0; i < N_GLIFOS; i++) {
        if (FUENTE[i].c == c) return FUENTE[i].col;
    }
    return FUENTE[0].col;   /* carácter no soportado -> espacio en blanco */
}


static esp_err_t oled_cmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}


esp_err_t oled_display_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = OLED_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_get_handle(), &dev_cfg, &s_dev);
    if (err != ESP_OK) return err;


    static const uint8_t init_seq[] = {
        0xAE,                    /* display off */
        0x20, 0x02,               /* addressing mode: horizontal (no crítico, usamos B0) */
        0xB0,                     /* page start = 0 */
        0xC8,                     /* COM scan dir remapeado */
        0x00, 0x10,                /* columna baja/alta = 0 */
        0x40,                     /* start line = 0 */
        0x81, 0x7F,                /* contraste */
        0xA1,                     /* segment remap */
        0xA6,                     /* display normal (no invertido) */
        0xA8, 0x3F,                /* multiplex ratio = 64 */
        0xA4,                     /* resume to RAM content display */
        0xD3, 0x00,                /* display offset = 0 */
        0xD5, 0xF0,                /* clock divide / oscillator freq */
        0xD9, 0x22,                /* precharge */
        0xDA, 0x12,                /* COM pins */
        0xDB, 0x20,                /* VCOMH deselect level */
        0x8D, 0x14,                /* habilita charge pump */
        0xAF,                     /* display on */
    };


    if (xSemaphoreTake(xI2cMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "Timeout esperando bus I2C para inicializar OLED");
        return ESP_ERR_TIMEOUT;
    }
    for (size_t i = 0; i < sizeof(init_seq); i++) {
        err = oled_cmd(init_seq[i]);
        if (err != ESP_OK) break;
    }
    xSemaphoreGive(xI2cMutex);


    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo inicializando el OLED (%s)", esp_err_to_name(err));
        return err;
    }


    memset(s_fb, 0, sizeof(s_fb));
    return ESP_OK;
}


static void oled_clear(void) { memset(s_fb, 0, sizeof(s_fb)); }


/* Dibuja texto en la fila "page" (0..7), arrancando en la columna en
 * píxeles "x0". Cada carácter ocupa 6 columnas (5 de la fuente + 1 de
 * separación). */
static void oled_draw_str(int page, int x0, const char *str)
{
    int x = x0;
    for (const char *p = str; *p && x < OLED_W - 5; p++, x += 6) {
        const uint8_t *g = buscar_glifo(*p);
        memcpy(&s_fb[page * OLED_W + x], g, 5);
    }
}


/* Envía el framebuffer completo al panel, página por página.
 * Protegido con xI2cMutex porque el bus lo comparte el DS3231. */
static esp_err_t oled_flush(void)
{
    if (xSemaphoreTake(xI2cMutex, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }


    esp_err_t err = ESP_OK;
    for (int page = 0; page < OLED_PAGES && err == ESP_OK; page++) {
        err = oled_cmd(0xB0 + page);
        if (err == ESP_OK) err = oled_cmd(0x00);
        if (err == ESP_OK) err = oled_cmd(0x10);
        if (err != ESP_OK) break;


        uint8_t buf[OLED_W + 1];
        buf[0] = 0x40;                              /* control byte: datos */
        memcpy(&buf[1], &s_fb[page * OLED_W], OLED_W);
        err = i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
    }


    xSemaphoreGive(xI2cMutex);
    return err;
}


static void linea_sensor(char *out, size_t n, const char *nom,
                         const datalog_entry_t *d, bool hay)
{
    if (!hay) {
        snprintf(out, n, "%s --", nom);
        return;
    }


    if (d->estado != ESTADO_OK) {
        snprintf(out, n, "%s DESC", nom);
        return;
    }


    const char *a = (d->alerta == ALERTA_TEMP_ALTA) ? "ALTA" :
                    (d->alerta == ALERTA_TEMP_BAJA) ? "BAJA" :
                    (d->alerta == ALERTA_CALENTANDO) ? "CAL" : "OK";


    snprintf(out, n, "%-6s %5.1fC %s",
             nom, d->temp_celsius, a);
}


void vTaskOled(void *pvParameters)
{
    datalog_entry_t ult[2] = {0};
    bool hay[2] = {false, false};
    char ip[16];
    char linea[40], hora[DATETIME_LEN];
    roast_info_t ri;
    TickType_t xLastWake = xTaskGetTickCount();


    for (;;) {
        datalog_entry_t dato;
        while (xQueueReceive(xOledQueue, &dato, 0) == pdTRUE) {
            ult[dato.sensor_id] = dato;
            hay[dato.sensor_id] = true;
        }


        ds3231_get_datetime_str(hora, sizeof(hora));
        roast_get_info(&ri);
        oled_clear();


        oled_draw_str(0, 0, hora);


        snprintf(linea, sizeof(linea), "%s %d-%dC", ri.nombre, (int)ri.temp_min, (int)ri.temp_max);
        oled_draw_str(1, 0, linea);
        bool sd_ok; uint32_t sd_n, sd_e;
        sd_logger_get_stats(&sd_ok, &sd_n, &sd_e);
        if (!sd_logger_disponible()) snprintf(linea, sizeof(linea), "NO SD");
        else if (!sd_ok)             snprintf(linea, sizeof(linea), "SD ERR");
        else                         snprintf(linea, sizeof(linea), "SD%04u", (unsigned)(sd_n % 10000));
        oled_draw_str(1, 92, linea);


        snprintf(linea, sizeof(linea), "T %02u:%02u %u-%u MIN",
                 (unsigned)(ri.elapsed_s / 60), (unsigned)(ri.elapsed_s % 60),
                 (unsigned)(ri.t_min_s / 60), (unsigned)(ri.t_max_s / 60));
        oled_draw_str(2, 0, linea);


        linea_sensor(linea, sizeof(linea), "CAFE", &ult[SENSOR_CAFE], hay[SENSOR_CAFE]);
        oled_draw_str(4, 0, linea);
        linea_sensor(linea, sizeof(linea), "TAMBOR", &ult[SENSOR_TAMBOR], hay[SENSOR_TAMBOR]);
        oled_draw_str(5, 0, linea);


        const char *msg = "PULSE START";
        if (ri.fase == TIEMPO_TOSTANDO)               msg = "TOSTANDO";
        else if (ri.fase == TIEMPO_VENTANA_DESCARGA)  msg = "LISTO PARA DESCARGAR";
        else if (ri.fase == TIEMPO_EXCEDIDO)          msg = "TIEMPO EXCEDIDO!";
        else if (ri.fase == TIEMPO_ENFRIANDO)         msg = "ENFRIANDO";
        oled_draw_str(7, 0, msg);

        const char *fx = (ri.fan == FAN_ENFRIAMIENTO) ? "FRIO" :
                         (ri.fan == FAN_CONTROL_TERMICO) ? "REG" : "OFF";
        snprintf(linea, sizeof(linea), "MOTOR %s FAN %s", motor_esta_girando() ? "ON" : "OFF", fx);
        oled_draw_str(6, 0, linea);

        if (web_server_get_ip(ip, sizeof(ip))) {
            snprintf(linea, sizeof(linea), "IP %s", ip);
        } else {
            snprintf(linea, sizeof(linea), "BUSCANDO RED");
        }
        oled_draw_str(3, 0, linea);


        if (oled_flush() != ESP_OK) ESP_LOGW(TAG, "Fallo al refrescar el OLED");
        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1000));
    }
}
