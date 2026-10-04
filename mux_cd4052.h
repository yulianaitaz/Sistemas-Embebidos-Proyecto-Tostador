#ifndef MUX_CD4052_H
#define MUX_CD4052_H


#include "esp_err.h"


esp_err_t mux_cd4052_init(void);


/* Selecciona el canal 0 (cafe) o 1 (tambor). Si el canal pedido ya
 * esta seleccionado, no hace nada (sin retardo). Si cambia de canal,
 * espera MUX_SETTLE_MS para que la senal analogica se asiente antes
 * de que el MAX6675 la lea. */
void mux_select_canal(int canal);


#endif /* MUX_CD4052_H */
