#include "ldr.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mqtt_tb.h"

// ======================= CONFIGURACION =======================
#define LDR_ADC_UNIT      ADC_UNIT_1
#define LDR_ADC_CHANNEL   ADC_CHANNEL_6       // GPIO34 (ADC1_CH6)
#define LDR_ADC_ATTEN     ADC_ATTEN_DB_12     // Rango 0-3.3 V
#define LDR_ADC_BITWIDTH  ADC_BITWIDTH_DEFAULT
#define LDR_PUBLISH_MS    5000                // Periodo de publicacion

#define N_MOVING          5                   // Num. de muestras de la media movil
// ============================================================

static const char *TAG = "LDR";

static adc_oneshot_unit_handle_t s_adc_handle  = NULL;
static adc_cali_handle_t         s_cali_handle = NULL;
static bool                      s_cali_enabled = false;

// ---------- Inicializacion ----------
esp_err_t ldr_init(void)
{
    if (s_adc_handle != NULL) {
        ESP_LOGW(TAG, "El ADC ya esta inicializado");
        return ESP_OK;
    }

    // 1. Crear unidad ADC1
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = LDR_ADC_UNIT,
    };
    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit fallo: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Configurar el canal del LDR
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = LDR_ADC_ATTEN,
        .bitwidth = LDR_ADC_BITWIDTH,
    };
    ret = adc_oneshot_config_channel(s_adc_handle, LDR_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_config_channel fallo: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Calibracion (v6.1: line_fitting)
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id  = LDR_ADC_UNIT,
        .atten    = LDR_ADC_ATTEN,
        .bitwidth = LDR_ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali_handle);
    if (ret == ESP_OK) {
        s_cali_enabled = true;
        ESP_LOGI(TAG, "Calibracion ADC habilitada (line_fitting)");
    } else {
        s_cali_enabled = false;
        ESP_LOGW(TAG, "Calibracion no disponible, se usara valor crudo");
    }

    return ESP_OK;
}

// ---------- Lectura cruda ----------
esp_err_t ldr_read_raw(int *raw)
{
    if (s_adc_handle == NULL || raw == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return adc_oneshot_read(s_adc_handle, LDR_ADC_CHANNEL, raw);
}

// ---------- Lectura en milivoltios ----------
esp_err_t ldr_read_mv(int *mv)
{
    if (mv == NULL) return ESP_ERR_INVALID_ARG;

    int raw = 0;
    esp_err_t ret = ldr_read_raw(&raw);
    if (ret != ESP_OK) return ret;

    if (s_cali_enabled) {
        return adc_cali_raw_to_voltage(s_cali_handle, raw, mv);
    }

    *mv = (raw * 3300) / 4095;
    return ESP_OK;
}

// ---------- Tarea FreeRTOS ----------
void ldr_task(void *param)
{
    if (ldr_init() != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo inicializar el ADC, la tarea termina");
        vTaskDelete(NULL);
        return;
    }

    int samples[N_MOVING] = {0};
    int idx    = 0;
    int filled = 0;

    while (1) {
        int raw = 0;
        esp_err_t ret = ldr_read_raw(&raw);

        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Error leyendo ADC: %s", esp_err_to_name(ret));
            vTaskDelay(pdMS_TO_TICKS(LDR_PUBLISH_MS));
            continue;
        }

        samples[idx] = raw;
        idx = (idx + 1) % N_MOVING;
        if (filled < N_MOVING) filled++;

        int sum = 0;
        for (int i = 0; i < filled; i++) sum += samples[i];
        int raw_avg = sum / filled;

        int mv = 0;
        if (s_cali_enabled) {
            adc_cali_raw_to_voltage(s_cali_handle, raw_avg, &mv);
            ESP_LOGI(TAG, "LDR raw=%d avg=%d V=%d mV", raw, raw_avg, mv);
        } else {
            ESP_LOGI(TAG, "LDR raw=%d avg=%d", raw, raw_avg);
        }

        tb_mqtt_publish_int("ldr", raw_avg);

        vTaskDelay(pdMS_TO_TICKS(LDR_PUBLISH_MS));
    }
}