#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inicializa y arranca el cliente MQTT de ThingsBoard.
 *
 * Debe llamarse una vez que el Wi-Fi esté conectado y con IP obtenida.
 * La función no bloquea: arranca el cliente en segundo plano.
 */
void tb_mqtt_start(void);

/**
 * @brief Publica un valor de telemetría (float) en ThingsBoard.
 *
 * @param key   Nombre de la variable en ThingsBoard (ej. "temperature").
 * @param value Valor a enviar.
 * @return esp_err_t ESP_OK si se encoló correctamente, ESP_FAIL si el cliente no está listo.
 */
esp_err_t tb_mqtt_publish_float(const char *key, float value);

/**
 * @brief Publica un valor de telemetría (int) en ThingsBoard.
 */
esp_err_t tb_mqtt_publish_int(const char *key, int value);

/**
 * @brief Publica una cadena JSON personalizada en el topic de telemetría.
 *
 * Ejemplo: tb_mqtt_publish_json("{\"temp\":25.4,\"hum\":60}");
 */
esp_err_t tb_mqtt_publish_json(const char *json_payload);

/**
 * @brief Devuelve true si el cliente MQTT está conectado a ThingsBoard.
 */
bool tb_mqtt_is_connected(void);

#ifdef __cplusplus
}
#endif

