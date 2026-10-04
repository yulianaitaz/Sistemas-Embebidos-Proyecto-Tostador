#ifndef CONFIG_PINES_H
#define CONFIG_PINES_H

#include "driver/gpio.h"
#include "driver/spi_master.h"

#define I2C_SDA_PIN         GPIO_NUM_21
#define I2C_SCL_PIN         GPIO_NUM_22
#define DS3231_I2C_ADDR     0x68
#define OLED_I2C_ADDR       0x3C


/* --- Bus SPI compartido: Termocupla MAX6675 + Modulo microSD --- */
#define PIN_NUM_MISO        GPIO_NUM_19   /* SO (MAX6675)  / DO (SD) */
#define PIN_NUM_MOSI        GPIO_NUM_23   /* no usado por MAX6675, sí por la SD (DI) */
#define PIN_NUM_CLK         GPIO_NUM_18   /* SCK compartido */
#define PIN_NUM_CS_MAX6675  GPIO_NUM_32  /* CS exclusivo de la termocupla */
#define PIN_NUM_CS_SD       GPIO_NUM_5    /* CS exclusivo de la microSD   */
#define SPI_HOST_ID         SPI3_HOST     


/* --- Multiplexor CD4052 (alterna entre termocupla cafe y tambor) ---
 * El CD4052 conmuta la senal ANALoGICA de las termocuplas hacia la unica entrada del MAX6675. Solo usamos 2 de sus 4 canales, asique
 * basta con el bit de seleccion A (el B se deja fijo en 0). */
#define MUX_SEL_A_PIN       GPIO_NUM_4    /* bit de seleccion de canal */
#define MUX_CANAL_CAFE      0
#define MUX_CANAL_TAMBOR    1
#define MUX_SETTLE_MS       300           /* asentamiento tras cambiar de canal */

#define PIN_BTN_START       GPIO_NUM_15  /* entrada pull-up: inicia/detiene el tueste */


/* --- LED RGB de estado  */
#define PIN_RGB_R           GPIO_NUM_13
#define PIN_RGB_G           GPIO_NUM_25
#define PIN_RGB_B           GPIO_NUM_14
#define RGB_COMMON_ANODE    0


/* --- Driver L298N: motor del tambor --- */
#define PIN_MOTOR_ENA       GPIO_NUM_27   /* PWM de velocidad */
#define MOTOR_VELOCIDAD_PCT 50           /* velocidad del tambor (0-100) */
#define MOTOR_RAMPA_MS      1500          /* arranque suave */

/* --- Rele del calefactor (modulo SRD-05VDC) --- */
#define PIN_RELE            GPIO_NUM_33
#define RELE_ACTIVO_BAJO    0     /* pon 1 si el LED SW queda encendido en reposo */

/* --- Ventilador: canal B del L298N (IN3 -> 3V3 e IN4 -> GND fijos por hardware) --- */
#define PIN_FAN_ENB         GPIO_NUM_26   /* PWM; con resistencia 10k a GND */
#define FAN_PCT_CONTROL     100           /* velocidad en control termico (0-100) */
#define FAN_PCT_ENFRIAR     100           /* velocidad en enfriamiento (0-100) */

#endif /* CONFIG_PINES_H */
