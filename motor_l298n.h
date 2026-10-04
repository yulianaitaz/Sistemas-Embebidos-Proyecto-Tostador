#ifndef MOTOR_L298N_H
#define MOTOR_L298N_H
#include <stdbool.h>
#include "esp_err.h"


esp_err_t motor_init(void);
void motor_start(void);          /* arranque suave */
void motor_stop(void);           /* frenado suave  */
bool motor_esta_girando(void);

void fan_set_pct(int pct);   /* 0 = apagado, 1..100 = velocidad (canal B del L298N) */
int  fan_pct(void);

#endif
