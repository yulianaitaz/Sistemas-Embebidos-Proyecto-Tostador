#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "config_pines.h"
#include "motor_l298n.h"

#define MOTOR_MODO   LEDC_LOW_SPEED_MODE
#define MOTOR_TIMER  LEDC_TIMER_0
#define MOTOR_CANAL  LEDC_CHANNEL_0
#define MOTOR_PWM_HZ 5000
#define FAN_CANAL    LEDC_CHANNEL_1      /* mismo timer que el motor */

static bool s_on = false;


static void mover_a(uint32_t duty, uint32_t ms)
{
    ledc_set_fade_with_time(MOTOR_MODO, MOTOR_CANAL, duty, ms);
    ledc_fade_start(MOTOR_MODO, MOTOR_CANAL, LEDC_FADE_NO_WAIT);   /* no bloquea */
}

static int s_fan_pct = 0;

static esp_err_t fan_init(void)
{
    ledc_channel_config_t ch = {
        .gpio_num = PIN_FAN_ENB, .speed_mode = MOTOR_MODO,
        .channel = FAN_CANAL, .timer_sel = MOTOR_TIMER, .duty = 0, .hpoint = 0,
    };
    return ledc_channel_config(&ch);
}

void fan_set_pct(int pct)
{
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    ledc_set_duty(MOTOR_MODO, FAN_CANAL, (pct * 255) / 100);
    ledc_update_duty(MOTOR_MODO, FAN_CANAL);
    s_fan_pct = pct;
}

int fan_pct(void) { return s_fan_pct; }

esp_err_t motor_init(void)
{
    esp_err_t err;
    
    ledc_timer_config_t t = {
        .speed_mode = MOTOR_MODO, .timer_num = MOTOR_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT, .freq_hz = MOTOR_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    err = ledc_timer_config(&t);
    if (err != ESP_OK) return err;


    ledc_channel_config_t ch = {
        .gpio_num = PIN_MOTOR_ENA, .speed_mode = MOTOR_MODO,
        .channel = MOTOR_CANAL, .timer_sel = MOTOR_TIMER, .duty = 0, .hpoint = 0,
    };
    err = ledc_channel_config(&ch);
    if (err != ESP_OK) return err;
     err = fan_init();
    if (err != ESP_OK) return err;

    return ledc_fade_func_install(0);
}


void motor_start(void)
{

    mover_a((MOTOR_VELOCIDAD_PCT * 255) / 100, MOTOR_RAMPA_MS);
    s_on = true;
}


void motor_stop(void)
{
    mover_a(0, 800);                     /* ENA a 0 = motor libre, sin energía */
    s_on = false;
}


bool motor_esta_girando(void) { return s_on; }
