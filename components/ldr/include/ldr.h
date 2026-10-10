#pragma once

#ifndef LDR_H
#define LDR_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa el ADC en modo oneshot para el LDR.
 *        Debe llamarse una sola vez antes de usar ldr_read_raw().
 */
esp_err_t ldr_init(void);

/**
 * @brief Lee el valor crudo del LDR (0–4095 con 12 bits).
 *
 * @param[out] raw  Puntero donde se guarda el valor leído.
 * @return ESP_OK si la lectura fue correcta.
 */
esp_err_t ldr_read_raw(int *raw);

/**
 * @brief Lee el LDR y devuelve el voltaje calibrado en milivoltios.
 *
 * @param[out] mv  Puntero donde se guarda el voltaje en mV.
 * @return ESP_OK si la lectura fue correcta.
 */
esp_err_t ldr_read_mv(int *mv);

/**
 * @brief Tarea FreeRTOS que lee el LDR cada 5 s y lo publica por MQTT.
 *        Se debe lanzar con xTaskCreate().
 */
void ldr_task(void *param);

#ifdef __cplusplus
}
#endif

#endif // LDR_H