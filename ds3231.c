#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ds3231.h"
#include "i2c_bus.h"
#include "config_pines.h"
#include "shared_resources.h"


static const char *TAG = "DS3231";
static i2c_master_dev_handle_t s_dev;


/* Snapshot: última hora leída + el instante (esp_timer) en que se leyó.
 * Así cualquier tarea puede calcular "ahora" sumando el tiempo
 * transcurrido, sin tener que tocar el bus I2C cada vez. */
typedef struct {
    time_t  epoch;
    int64_t read_us;
    bool    valid;
} rtc_snapshot_t;


static rtc_snapshot_t     s_snap = {0};
static SemaphoreHandle_t  s_snap_mutex;


static inline uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }


static esp_err_t ds3231_read_raw(struct tm *t)
{
    uint8_t reg = 0x00;
    uint8_t b[7];
    esp_err_t err = i2c_master_transmit_receive(s_dev, &reg, 1, b, sizeof(b), 100);
    if (err != ESP_OK) return err;


    t->tm_sec = bcd2dec(b[0] & 0x7F);
    t->tm_min = bcd2dec(b[1] & 0x7F);
    if (b[2] & 0x40) {                         /* formato 12 h */
        int h = bcd2dec(b[2] & 0x1F);
        if (h == 12) h = 0;
        if (b[2] & 0x20) h += 12;
        t->tm_hour = h;
    } else {
        t->tm_hour = bcd2dec(b[2] & 0x3F);     /* formato 24 h */
    }
    t->tm_mday  = bcd2dec(b[4] & 0x3F);
    t->tm_mon   = bcd2dec(b[5] & 0x1F) - 1;
    t->tm_year  = bcd2dec(b[6]) + 100;
    t->tm_isdst = 0;
    return ESP_OK;
}


esp_err_t ds3231_init(void)
{
    s_snap_mutex = xSemaphoreCreateMutex();
    if (!s_snap_mutex) return ESP_ERR_NO_MEM;


    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = DS3231_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    return i2c_master_bus_add_device(i2c_bus_get_handle(), &dev_cfg, &s_dev);
}


void ds3231_get_datetime_str(char *out, size_t n)
{
    rtc_snapshot_t snap = {0};


    if (xSemaphoreTake(s_snap_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
        snap = s_snap;
        xSemaphoreGive(s_snap_mutex);
    }


    if (!snap.valid) {
        snprintf(out, n, "1970-01-01 00:00:00");
        return;
    }
    time_t now = snap.epoch + (time_t)((esp_timer_get_time() - snap.read_us) / 1000000);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(out, n, "%Y-%m-%d %H:%M:%S", &tm_now);
}


void vTaskRtc(void *pvParameters)
{
    TickType_t xLastWake = xTaskGetTickCount();


    for (;;) {
        struct tm t;
        esp_err_t err = ESP_FAIL;


        if (xSemaphoreTake(xI2cMutex, pdMS_TO_TICKS(2000)) == pdTRUE) {
            err = ds3231_read_raw(&t);
            xSemaphoreGive(xI2cMutex);
        } else {
            ESP_LOGW(TAG, "Timeout esperando bus I2C");
        }


        if (err == ESP_OK) {
            rtc_snapshot_t snap = {
                .epoch   = mktime(&t),
                .read_us = esp_timer_get_time(),
                .valid   = true,
            };
            if (xSemaphoreTake(s_snap_mutex, pdMS_TO_TICKS(500)) == pdTRUE) {
                s_snap = snap;
                xSemaphoreGive(s_snap_mutex);
            }
        } else {
            ESP_LOGW(TAG, "Fallo lectura DS3231 (%s)", esp_err_to_name(err));
        }


        vTaskDelayUntil(&xLastWake, pdMS_TO_TICKS(1000));
    }
}
