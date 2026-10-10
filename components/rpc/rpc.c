#include <stdio.h>
#include "cJSON.h"
#include "rpc.h"


// ======================= CONFIGURACION =======================
#define TB_ACCESS_TOKEN    "5w51m0m9gqzisjpfa12x"
#define TB_BROKER_URI      "mqtt://mqtt.eu.thingsboard.cloud:1883"
#define TB_TOPIC_TELEMETRY "v1/devices/me/rpc/request"  // Topic para recibir RPC desde ThingsBoard
// ============================================================

volatile int rpc_value = 0;  // Dato Variable global para almacenar el valor recibido a través de RPC


void func(void)
{

}
