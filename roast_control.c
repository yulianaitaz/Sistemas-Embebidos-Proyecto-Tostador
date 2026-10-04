#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_pines.h"
#include "roast_control.h"
#include "rgb_led.h"
#include "motor_l298n.h"
#include "heater_relay.h"



static const char *TAG = "ROAST";
static fan_modo_t s_fan = FAN_APAGADO;
static float      s_temp[2];
static bool       s_val[2];
static int64_t    s_us[2];
static int64_t    s_t_enf_us = 0;
static int64_t    s_falla_desde_us = 0;
static volatile tueste_t s_tueste = TUESTE_CLARO;
static volatile bool     s_run = false;
static volatile int64_t  s_t0_us = 0;
static bool              s_alcanzo[2];
static volatile alerta_t s_alerta[2];
static uint32_t          s_inicio_ticks = 0;   /* destello cian al iniciar */
static volatile bool s_enfriando = false;
static uint32_t      s_t_fin_s = 0;       /* cronometro congelado al terminar el tueste */
static int64_t s_ultimo_cafe_us = 0;
static int64_t s_ultimo_cambio_us = 0;
static volatile bool s_fan_test = false;   /* prueba manual del ventilador */


#define LONG_PRESS_TICKS 20      /* 20 x 50 ms = 1 s */
/* ---------- Control del calefactor (on/off con histeresis) ---------- */
#define HIST_C          2.0f
#define TEMP_ABS_MAX_C  265.0f
#define STALE_US        (15LL * 1000000LL)   /* sin lectura del cafe en 15 s -> apagar */
#define MIN_APAGADO_US  (3LL * 1000000LL)    /* descanso minimo del rele antes de reencender */
#define FAN_HIST_C        5.0f
#define FAN_T_MIN_DIV     2
#define SEG_MARGEN_C      15.0f
#define SEG_USA_TAMBOR    1         /* 1 = el tambor tambien dispara la seguridad */
#define FIN_EN_T_MAX      0         /* 0 = fin al llegar a t_min; 1 = fin en t_max */
#define FIN_ENFRIAR_C     50.0f     /* temperatura segura del cafe */
#define ENFRIAR_MAX_S     900       /* tope de seguridad del enfriamiento */
#define FALLA_SENSOR_US   (30LL * 1000000LL)
#define STALE_TAMBOR_US   (25LL * 1000000LL)   /* el tambor se lee cada 10 s */


typedef struct {
    const char *nombre;
    float temp_min, temp_max;
    uint32_t t_min_s, t_max_s;
} perfil_t;


static const perfil_t PERFILES[TUESTE_N] = {
    { "CLARO",  180.0f, 197.0f,  7 * 60, 11 * 60 },
    { "MEDIO",  210.0f, 220.0f,  7 * 60, 13 * 60 },
    { "OSCURO", 230.0f, 250.0f, 12 * 60, 15 * 60 },
};

static uint32_t elapsed_s(void)
{
    if (s_run) return (uint32_t)((esp_timer_get_time() - s_t0_us) / 1000000);
    return s_enfriando ? s_t_fin_s : 0;
}


static fase_tiempo_t fase_actual(void)
{
    if (s_enfriando) return TIEMPO_ENFRIANDO;
    if (!s_run) return TIEMPO_DETENIDO;
    const perfil_t *p = &PERFILES[s_tueste];
    uint32_t t = elapsed_s();
    if (t > p->t_max_s) return TIEMPO_EXCEDIDO;
    if (t >= p->t_min_s) return TIEMPO_VENTANA_DESCARGA;
    return TIEMPO_TOSTANDO;
}
static void calentador(bool on)
{
    if (on == rele_esta_encendido()) return;
    int64_t ahora = esp_timer_get_time();
    if (on && (ahora - s_ultimo_cambio_us) < MIN_APAGADO_US) return;   /* apagar es siempre inmediato */
    rele_set(on);
    s_ultimo_cambio_us = ahora;
    ESP_LOGI(TAG, "Calefactor %s", on ? "ENCENDIDO" : "APAGADO");
}

static void calentador_control(float t, estado_sensor_t est)
{
    const perfil_t *p = &PERFILES[s_tueste];
    float sp = (p->temp_min + p->temp_max) / 2.0f;

    bool permitido = s_run && motor_esta_girando() && est == ESTADO_OK &&
                     t < TEMP_ABS_MAX_C && fase_actual() != TIEMPO_EXCEDIDO;
    if (!permitido) { calentador(false); return; }

    if (t <= sp - HIST_C)      calentador(true);
    else if (t >= sp + HIST_C) calentador(false);
}

static void fan_modo(fan_modo_t m)
{
    if (m == s_fan) return;
    s_fan = m;
    fan_set_pct(m == FAN_APAGADO ? 0 : (m == FAN_ENFRIAMIENTO ? FAN_PCT_ENFRIAR : FAN_PCT_CONTROL));
    ESP_LOGI(TAG, "Ventilador: %s", m == FAN_APAGADO ? "APAGADO" :
             (m == FAN_ENFRIAMIENTO ? "ENFRIAMIENTO" : "CONTROL TERMICO"));
}

static void iniciar_enfriamiento(const char *motivo)
{
    calentador(false);                       /* 1) calefaccion OFF */
    s_t_fin_s = elapsed_s();                 /* congela el cronometro (antes de s_run=false) */
    s_run = false;
    s_enfriando = true;
    s_t_enf_us = esp_timer_get_time();
    s_alerta[0] = s_alerta[1] = ALERTA_NINGUNA;
    fan_modo(FAN_ENFRIAMIENTO);              /* 2) ventilador ON */
    if (!motor_esta_girando()) motor_start();/* 3) motor ON */
    ESP_LOGW(TAG, "ENFRIAMIENTO: %s", motivo);
}

static void terminar_enfriamiento(const char *motivo)
{
    s_enfriando = false;
    s_t_fin_s = 0;
    fan_modo(FAN_APAGADO);
    motor_stop();
    ESP_LOGI(TAG, "Fin del ciclo: %s", motivo);
}

/* Se llama cada 50 ms desde vTaskRoast */
static void supervisar(void)
{
    int64_t ahora = esp_timer_get_time();
    const perfil_t *p = &PERFILES[s_tueste];
    bool cafe_ok = s_val[SENSOR_CAFE]   && (ahora - s_us[SENSOR_CAFE])   < STALE_US;
    bool tamb_ok = s_val[SENSOR_TAMBOR] && (ahora - s_us[SENSOR_TAMBOR]) < STALE_TAMBOR_US;

    /* ---- Fase de enfriamiento ---- */
    if (s_enfriando) {
        fan_modo(FAN_ENFRIAMIENTO);
        bool frio = false;
        if (cafe_ok)      frio = s_temp[SENSOR_CAFE]   <= FIN_ENFRIAR_C;
        else if (tamb_ok) frio = s_temp[SENSOR_TAMBOR] <= FIN_ENFRIAR_C;
        if (frio) terminar_enfriamiento("temperatura segura");
        else if ((ahora - s_t_enf_us) > (int64_t)ENFRIAR_MAX_S * 1000000LL)
            terminar_enfriamiento("tiempo maximo de enfriamiento");
        return;
    }

    /* ---- Inactivo ---- */
    if (!s_run) { s_falla_desde_us = 0; if (!s_fan_test) fan_modo(FAN_APAGADO); return; }
    /* ---- Tueste en marcha ---- */
    uint32_t t = elapsed_s();
    float t_seg = p->temp_max + SEG_MARGEN_C;
    if (t_seg > TEMP_ABS_MAX_C) t_seg = TEMP_ABS_MAX_C;

    if (!cafe_ok) {                          /* falla o sensor congelado: calor ya esta OFF */
        if (s_falla_desde_us == 0) s_falla_desde_us = ahora;
        fan_modo(FAN_CONTROL_TERMICO);       /* el ventilador ayuda a enfriar */
        if ((ahora - s_falla_desde_us) > FALLA_SENSOR_US) iniciar_enfriamiento("falla de sensor");
        return;
    }
    s_falla_desde_us = 0;

    float tc = s_temp[SENSOR_CAFE];

    /* 1) Seguridad: prioridad maxima */
    if (tc >= t_seg || (SEG_USA_TAMBOR && tamb_ok && s_temp[SENSOR_TAMBOR] >= t_seg)) {
        iniciar_enfriamiento("temperatura de seguridad");
        return;
    }

    /* 2) Fin del tueste segun perfil */
    uint32_t t_fin = FIN_EN_T_MAX ? p->t_max_s : p->t_min_s;
    if ((t >= t_fin && tc >= p->temp_min) || t >= p->t_max_s) {
        iniciar_enfriamiento("fin del tueste por perfil");
        return;
    }

    /* 3) Control termico con histeresis */
    if (t >= p->t_min_s / FAN_T_MIN_DIV && tc >= p->temp_max)
        fan_modo(FAN_CONTROL_TERMICO);
    else if (s_fan == FAN_CONTROL_TERMICO && tc <= p->temp_max - FAN_HIST_C)
        fan_modo(FAN_APAGADO);
}


esp_err_t roast_control_init(void)
{
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_BTN_START),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&in);
    if (err != ESP_OK) return err;

    err = rgb_led_init();
    if (err != ESP_OK) return err;

    err = rele_init();
    if (err != ESP_OK) return err;
    return motor_init();
}

alerta_t roast_evaluar(sensor_id_t id, float t, estado_sensor_t est)
{
    alerta_t a;
    const perfil_t *p = &PERFILES[s_tueste];


    if (est != ESTADO_OK)      a = ALERTA_DESCONECTADA;
    else if (!s_run)           a = ALERTA_NINGUNA;
    else if (t > p->temp_max)  { s_alcanzo[id] = true; a = ALERTA_TEMP_ALTA; }
    else if (t >= p->temp_min) { s_alcanzo[id] = true; a = ALERTA_NINGUNA; }
    else if (s_alcanzo[id] || elapsed_s() > p->t_min_s) a = ALERTA_TEMP_BAJA;
    else                       a = ALERTA_CALENTANDO;


    if (a != s_alerta[id]) {
        s_alerta[id] = a;
        if (a == ALERTA_TEMP_ALTA || a == ALERTA_TEMP_BAJA || a == ALERTA_DESCONECTADA)
            ESP_LOGW(TAG, "ALERTA %s: tipo=%d temp=%.1f (rango %.0f-%.0f)",
                     id == SENSOR_CAFE ? "CAFE" : "TAMBOR", (int)a, t, p->temp_min, p->temp_max);
    }

    s_temp[id] = t;
    s_val[id]  = (est == ESTADO_OK);
    s_us[id]   = esp_timer_get_time();

    if (id == SENSOR_CAFE) {
        s_ultimo_cafe_us = esp_timer_get_time();
        calentador_control(t, est);
    }
    return a;
}


void roast_get_info(roast_info_t *o)
{
    const perfil_t *p = &PERFILES[s_tueste];
    o->tueste = s_tueste;     o->nombre = p->nombre;
    o->temp_min = p->temp_min; o->temp_max = p->temp_max;
    o->t_min_s = p->t_min_s;  o->t_max_s = p->t_max_s;
    o->elapsed_s = elapsed_s(); o->fase = fase_actual();
    o->fan = s_fan;
}


const char *roast_nombre_tueste(uint8_t t) { return PERFILES[t % TUESTE_N].nombre; }


static void iniciar_tueste(void)
{
    s_fan_test = false;
    fan_modo(FAN_APAGADO);
    s_alcanzo[0] = s_alcanzo[1] = false;
    s_alerta[0] = s_alerta[1] = ALERTA_CALENTANDO;
    s_t0_us = esp_timer_get_time();
    s_run = true;
    s_ultimo_cafe_us = esp_timer_get_time();
    s_inicio_ticks = 20;                 /* 1 s en cian */
    motor_start();
    ESP_LOGI(TAG, "Inicio tueste %s (motor ON)", PERFILES[s_tueste].nombre);
}


static void detener_tueste(void)
{
    iniciar_enfriamiento("tueste terminado por el usuario");
}

bool roast_toggle(void)
{
    if (s_enfriando)  terminar_enfriamiento("cancelado por el usuario");
    else if (!s_run)  iniciar_tueste();
    else              detener_tueste();
    return s_run;
}

bool roast_set_tueste(int n)
{
    if (s_run || s_enfriando || n < 0 || n >= TUESTE_N) return false;
    s_tueste = (tueste_t)n;
    ESP_LOGI(TAG, "Tueste (web): %s", PERFILES[n].nombre);
    return true;
}

bool roast_fan_test_toggle(void)
{
    if (s_run || s_enfriando) return false;     /* solo con el tueste detenido */
    s_fan_test = !s_fan_test;
    fan_modo(s_fan_test ? FAN_ENFRIAMIENTO : FAN_APAGADO);   /* 100 % */
    ESP_LOGI(TAG, "Prueba de ventilador: %s", s_fan_test ? "ON" : "OFF");
    return true;
}

void vTaskRoast(void *pv)
{
    bool bs_prev = true, long_done = false;
    uint32_t tick = 0, press = 0;
    fase_tiempo_t fase_prev = TIEMPO_DETENIDO;


    for (;;) {
        /* --- Un solo botón: corto = inicia/detiene, largo = cambia tueste --- */
        bool bs = gpio_get_level(PIN_BTN_START);
        if (!bs) {
            press++;
            if (press == LONG_PRESS_TICKS && !s_run && !s_enfriando) {
                s_tueste = (tueste_t)((s_tueste + 1) % TUESTE_N);
                long_done = true;
                ESP_LOGI(TAG, "Tueste: %s", PERFILES[s_tueste].nombre);
            }
        } else {
            if (!bs_prev && !long_done && press >= 2) {
                roast_toggle();
            }
            press = 0;
            long_done = false;
        }
        bs_prev = bs;


        fase_tiempo_t fase = fase_actual();
        if (fase != fase_prev) {
            if (fase == TIEMPO_VENTANA_DESCARGA) { ESP_LOGW(TAG, "Tiempo minimo cumplido: se puede descargar"); }
            if (fase == TIEMPO_EXCEDIDO)ESP_LOGW(TAG, "TIEMPO MAXIMO EXCEDIDO");
            fase_prev = fase;
        }


        bool alerta_temp = false, calentando = false;
        for (int i = 0; i < 2; i++) {
            if (s_alerta[i] == ALERTA_TEMP_ALTA || s_alerta[i] == ALERTA_TEMP_BAJA ||
                s_alerta[i] == ALERTA_DESCONECTADA) alerta_temp = true;
            if (s_alerta[i] == ALERTA_CALENTANDO) calentando = true;
        }

        /* --- RGB: la prioridad va de arriba hacia abajo --- */
        rgb_color_t col;
        if (fase == TIEMPO_EXCEDIDO)            col = ((tick % 4) < 2) ? RGB_ROJO : RGB_OFF;  /* rojo parpadeante */
        else if (alerta_temp)                   col = RGB_ROJO;                                /* umbral pasado / falla */
        else if (fase == TIEMPO_ENFRIANDO)      col = ((tick % 20) < 10) ? RGB_CIAN : RGB_OFF;  /* enfriando */
        else if (!s_run)                        col = RGB_AZUL;                                /* en espera */
        else if (s_inicio_ticks)                { col = RGB_CIAN; s_inicio_ticks--; }          /* recién iniciado */
        else if (fase == TIEMPO_VENTANA_DESCARGA) col = ((tick % 10) < 5) ? RGB_MAGENTA : RGB_OFF; /* listo para descargar */
        else if (calentando)                    col = RGB_AMARILLO;                            /* calentando */
        else                                    col = RGB_VERDE;                               /* en rango, todo bien */
        rgb_led_set(col);


        if (rele_esta_encendido() &&
            (!s_run || (esp_timer_get_time() - s_ultimo_cafe_us) > STALE_US)) {
            calentador(false);
        }

        supervisar();


        tick++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
