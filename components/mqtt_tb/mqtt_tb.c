#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "mqtt_tb.h"

#include "cJSON.h"   // espidf ver 5.x.x  

#include "esp_log.h"
#include "mqtt_client.h"   // Libreria MQTT de ESP-IDF Dependencia Importante

static const char *TAG = "TB_MQTT";

// ======================= CONFIGURACION =======================
#define TB_ACCESS_TOKEN    "5w51m0m9gqzisjpfa12x"
#define TB_BROKER_URI      "mqtt://mqtt.eu.thingsboard.cloud:1883"
#define TB_TOPIC_TELEMETRY "v1/devices/me/telemetry"
#define TB_TOPIC_RPC       "v1/devices/me/rpc/request/+"
// ============================================================

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static volatile bool s_connected = false;    // Volatile indicar que la variable puede ser modificada por interrupciones o 
                                            // hilos, evitando optimizaciones del compilador, evitar problemas de sincronización.


volatile uint16_t valor = 0;  // RPC dato


// ---------- Manejador de Eventos ----------
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) { // Maquina de estado de eventos
        case MQTT_EVENT_CONNECTED:  
            s_connected = true;
            ESP_LOGI(TAG, "Conectado a ThingsBoard");

            esp_mqtt_client_subscribe(event->client, TB_TOPIC_RPC, 1);  // RPC: escuchar peticiones   ****

            break;
/*
typedef struct cJSON {
    struct cJSON *next, *prev; // Para listas enlazadas (arrays u objetos)
    struct cJSON *child;      // Si es un array u objeto, apunta al primer elemento
    int type;                 // Tipo de dato (cJSON_Number, cJSON_String, etc.)
    char *valuestring;        // Valor si es texto
    int valueint;             // Valor si es entero
    double valuedouble;       // Valor si es flotante
    char *string;             // Clave/Nombre de la propiedad en un objeto
} cJSON;
 */
        case MQTT_EVENT_DATA: {   // RPC one-way                                                      ****
            // payload = {"method":"setValor","params":30}
            cJSON *root   = cJSON_ParseWithLength(event->data, event->data_len);
            cJSON *method = cJSON_GetObjectItem(root, "method");
            cJSON *params = cJSON_GetObjectItem(root, "params"); 
            /*cJSON_GetObjectItem(const cJSON * const object, const char * const string): sensible a mayúsculas/minúsculas).
              cJSON_GetObjectItemCaseSensitive(...): comparación estricta de mayúsculas/minúsculas.*/
 
            if (strcmp(method->valuestring, "setValor") == 0) {
                valor = params->valueint;
                ESP_LOGI(TAG, "RPC setValor valor = %u", valor);
            }
            cJSON_Delete(root); cJSON_Delete(method); cJSON_Delete(params);  // Liberar memoria 
            break;
        }


        case MQTT_EVENT_DISCONNECTED: 
            s_connected = false;
            ESP_LOGW(TAG, "Desconectado de ThingsBoard");
            break;

        case MQTT_EVENT_ERROR: 
            s_connected = false;
            ESP_LOGE(TAG, "Error MQTT");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "Conexión rechazada, return code: 0x%x",
                         event->error_handle->connect_return_code);
            }
            break;

        default:
            break;
    }
}

// ---------- API Publica ----------
void tb_mqtt_start(void)
{
    if (s_mqtt_client != NULL) {
        ESP_LOGW(TAG, "El cliente MQTT ya está inicializado");
        return;
    }

    esp_mqtt_client_config_t mqtt_cfg = {    // Struc de configuracion MQTT
        .broker.address.uri     = TB_BROKER_URI,   // URI del BROKER
        .credentials.username   = TB_ACCESS_TOKEN  // Token usado como usuario
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);     // Iniciar MQTT obtener handle
    if (s_mqtt_client == NULL) { 
        ESP_LOGE(TAG, "No se pudo inicializar el cliente MQTT");
        return;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);  // hadler funcion
    esp_mqtt_client_start(s_mqtt_client); // Conectar al broker MQTT

    ESP_LOGI(TAG, "Cliente MQTT arrancado");
}


esp_err_t tb_mqtt_publish_float(const char *key, float value)
{
    if (s_mqtt_client == NULL || !s_connected) { // Verificacion
        ESP_LOGW(TAG, "MQTT no conectado, no se puede publicar");
        return ESP_FAIL;
    }

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"%s\":%.2f}", key, value); 

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, TB_TOPIC_TELEMETRY,
                                         payload, 0, 1, 0); // Plubicar mensaje al broker MQTT

    if (msg_id < 0) {           // Verificacion de error por si el mensaje por main no es correcto
        ESP_LOGE(TAG, "Error al publicar: %s", payload);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Publicado: %s (msg_id=%d)", payload, msg_id);
    return ESP_OK;
}

esp_err_t tb_mqtt_publish_int(const char *key, int value)
{
    if (s_mqtt_client == NULL || !s_connected) {
        ESP_LOGW(TAG, "MQTT no conectado, no se puede publicar");
        return ESP_FAIL;
    }

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"%s\":%d}", key, value);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, TB_TOPIC_TELEMETRY,
                                         payload, 0, 1, 0); // 0 = longitud, 1 = QoS, 0 = Retain 
    if (msg_id < 0) {                                       
        ESP_LOGE(TAG, "Error al publicar: %s", payload);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Publicado: %s (msg_id=%d)", payload, msg_id);
    return ESP_OK;
}

esp_err_t tb_mqtt_publish_json(const char *json_payload)   // Publicar un mensaje JSON completo
{
    if (s_mqtt_client == NULL || !s_connected) {
        ESP_LOGW(TAG, "MQTT no conectado, no se puede publicar");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, TB_TOPIC_TELEMETRY,  
                                         json_payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Error al publicar: %s", json_payload);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Publicado: %s (msg_id=%d)", json_payload, msg_id);
    return ESP_OK;
}

bool tb_mqtt_is_connected(void)   // Estado de conexion MQTT
{
    return s_connected;
}