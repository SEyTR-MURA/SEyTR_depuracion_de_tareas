/* carga.c - variante 2: la carga del procesador medida con la tarea inactiva.
 *
 * Registra con esp_register_freertos_idle_hook_for_cpu() una función de
 * enganche en la tarea inactiva de cada núcleo. Cada función cuenta sus
 * llamadas y devuelve false, para que la tarea inactiva la llame tan deprisa
 * como pueda. Si todas las funciones de enganche de un núcleo devolvieran
 * true, la tarea inactiva pararía el procesador hasta la siguiente
 * interrupción y solo las llamaría una vez por interrupción (freertos_hooks.c
 * de ESP-IDF); en el núcleo hay otra, la del watchdog de tareas, que devuelve
 * true. A cambio, mientras esta variante funciona, los núcleos no se paran en
 * reposo.
 *
 * Antes de crear la tarea de carga, el programa cuenta durante 1 s cuántas
 * vueltas da cada tarea inactiva con el núcleo casi libre: es la referencia.
 * Después, cada 2 s, la capacidad libre de cada núcleo es lo que ha contado
 * en ese tiempo dividido entre lo que contaría con la referencia, y la carga
 * es el resto. El informe da también la capacidad libre y la carga que salen
 * del tiempo que las estadísticas de ejecución de FreeRTOS atribuyen a cada
 * tarea inactiva en el mismo intervalo. El primer informe solo toma la
 * primera lectura.
 *
 * La tarea de carga, en el núcleo 0, se activa cada 100 ms y calcula durante
 * el porcentaje elegido de ese periodo. La tecla «c» cambia el porcentaje.
 */

#include "comun.h"

#include <stdio.h>

#include "esp_freertos_hooks.h"
#include "esp_timer.h"

#define PERIODO_CARGA_MS   100
#define CALIBRACION_MS     1000
#define MAX_TAREAS         32

static volatile uint32_t s_vueltas[2];
static uint32_t s_referencia[2];      /* vueltas por segundo, casi sin carga */
static volatile int s_nivel = 0;      /* porcentaje de carga del núcleo 0 */
static const int NIVELES[] = { 0, 25, 50, 75 };

/* Las funciones de enganche se ejecutan en la tarea inactiva: no pueden
 * bloquearse ni tardar. Solo cuentan. */
static bool gancho_nucleo0(void)
{
    s_vueltas[0] = s_vueltas[0] + 1;
    return false;  /* que la tarea inactiva vuelva a llamarla enseguida */
}

static bool gancho_nucleo1(void)
{
    s_vueltas[1] = s_vueltas[1] + 1;
    return false;
}

static void tarea_carga(void *argumento)
{
    (void)argumento;
    TickType_t referencia = xTaskGetTickCount();
    for (;;) {
        trabajar_us((int64_t)s_nivel * PERIODO_CARGA_MS * 1000 / 100);
        xTaskDelayUntil(&referencia, pdMS_TO_TICKS(PERIODO_CARGA_MS));
    }
}

/* Lo último que se midió, para calcular la diferencia en cada informe. */
static uint32_t s_vueltas_antes[2];
static configRUN_TIME_COUNTER_TYPE s_inactiva_antes[2];
static configRUN_TIME_COUNTER_TYPE s_total_antes;
static TaskStatus_t s_estado[MAX_TAREAS];

/* Tiempo de ejecución acumulado de la tarea inactiva de cada núcleo, y el
 * total, con uxTaskGetSystemState(), que es lo que usa por dentro
 * vTaskGetRunTimeStats(). */
static void leer_inactivas(configRUN_TIME_COUNTER_TYPE inactiva[2],
                           configRUN_TIME_COUNTER_TYPE *total)
{
    const TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    const TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);
    *total = 0;
    inactiva[0] = inactiva[1] = 0;
    /* Devuelve 0, sin escribir nada, si hay más tareas que sitios. */
    const UBaseType_t n = uxTaskGetSystemState(s_estado, MAX_TAREAS, total);
    for (UBaseType_t i = 0; i < n; i++) {
        if (s_estado[i].xHandle == idle0) {
            inactiva[0] = s_estado[i].ulRunTimeCounter;
        } else if (s_estado[i].xHandle == idle1) {
            inactiva[1] = s_estado[i].ulRunTimeCounter;
        }
    }
}

void carga_crear(void)
{
    if (esp_register_freertos_idle_hook_for_cpu(gancho_nucleo0, 0) != ESP_OK
        || esp_register_freertos_idle_hook_for_cpu(gancho_nucleo1, 1) != ESP_OK) {
        printf("No se han podido registrar las funciones de enganche.\n");
        return;
    }

    /* Referencia: vueltas en 1 s con el núcleo casi libre. La tarea del
     * informe y la del teclado están bloqueadas mientras tanto. */
    printf("Midiendo la referencia durante %d ms, sin carga...\n", CALIBRACION_MS);
    const uint32_t v0 = s_vueltas[0];
    const uint32_t v1 = s_vueltas[1];
    vTaskDelay(pdMS_TO_TICKS(CALIBRACION_MS));
    s_referencia[0] = (uint32_t)((uint64_t)(s_vueltas[0] - v0) * 1000u / CALIBRACION_MS);
    s_referencia[1] = (uint32_t)((uint64_t)(s_vueltas[1] - v1) * 1000u / CALIBRACION_MS);
    printf("Referencia: nucleo 0, %lu vueltas/s; nucleo 1, %lu vueltas/s.\n",
           (unsigned long)s_referencia[0], (unsigned long)s_referencia[1]);

    if (xTaskCreatePinnedToCore(tarea_carga, "carga", PILA_TAREA, NULL, 4, NULL,
                                NUCLEO_PRUEBA) != pdPASS) {
        printf("No hay memoria para la tarea de carga.\n");
    }
}

static bool s_primera = true;

void carga_informe(void)
{
    const uint32_t vueltas[2] = { s_vueltas[0], s_vueltas[1] };
    configRUN_TIME_COUNTER_TYPE inactiva[2];
    configRUN_TIME_COUNTER_TYPE total;
    leer_inactivas(inactiva, &total);
    if (s_primera || total == 0) {  /* primera lectura: no hay intervalo */
        s_primera = false;
        s_vueltas_antes[0] = vueltas[0];
        s_vueltas_antes[1] = vueltas[1];
        s_inactiva_antes[0] = inactiva[0];
        s_inactiva_antes[1] = inactiva[1];
        s_total_antes = total;
        return;
    }

    /* El total es el tiempo transcurrido, en microsegundos con el reloj de
     * esp_timer, que es el de las estadísticas por omisión. Las restas sin signo admiten que el contador dé la vuelta
     * una vez entre dos informes. */
    const uint32_t dt_us = (uint32_t)(total - s_total_antes);
    printf("\nCarga pedida al nucleo 0: %d %%. En los ultimos %lu ms:\n",
           s_nivel, (unsigned long)(dt_us / 1000));
    for (int c = 0; c < 2; c++) {
        const uint32_t dv = vueltas[c] - s_vueltas_antes[c];
        const uint32_t esperadas = (uint32_t)((uint64_t)s_referencia[c] * dt_us / 1000000u);
        const unsigned libre_gancho = esperadas ? (unsigned)((uint64_t)dv * 100u / esperadas) : 0;
        const unsigned libre_estad = dt_us ? (unsigned)((uint64_t)(uint32_t)(inactiva[c] - s_inactiva_antes[c])
                                                        * 100u / dt_us) : 0;
        printf("  nucleo %d: con la funcion de enganche, libre %3u %% y carga %4d %%;"
               " con las estadisticas, libre %3u %% y carga %3d %%\n",
               c, libre_gancho, 100 - (int)libre_gancho, libre_estad, 100 - (int)libre_estad);
    }
    s_vueltas_antes[0] = vueltas[0];
    s_vueltas_antes[1] = vueltas[1];
    s_inactiva_antes[0] = inactiva[0];
    s_inactiva_antes[1] = inactiva[1];
    s_total_antes = total;
}

bool carga_tecla(char tecla)
{
    if (tecla != 'c') {
        return false;
    }
    int i = 0;
    while (NIVELES[i] != s_nivel) {
        i++;
    }
    s_nivel = NIVELES[(i + 1) % (int)(sizeof NIVELES / sizeof NIVELES[0])];
    printf("\nCarga del nucleo 0: %d %% de cada %d ms.\n", s_nivel, PERIODO_CARGA_MS);
    return true;
}
