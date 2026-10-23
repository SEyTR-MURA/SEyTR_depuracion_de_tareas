/* vigilancia.c - variantes 3, 4 y 5: el watchdog de tareas.
 *
 * Con la configuración de partida de ESP-IDF, el watchdog de tareas vigila
 * la tarea inactiva de cada núcleo: si en 5 s alguna no ha llegado a
 * ejecutarse, escribe un aviso con las tareas que no le han dado señal y las
 * que se estaban ejecutando, y el programa sigue.
 *
 *   3  aviso del watchdog: la tarea «acaparadora», de prioridad 5 en el
 *      núcleo 0, deja de bloquearse con la tecla «b». La tarea inactiva de
 *      ese núcleo no llega a ejecutarse y aparece el aviso.
 *   4  tarea vigilada: la tarea «vigilada» se suscribe al watchdog con
 *      esp_task_wdt_add() y le da señal con esp_task_wdt_reset() en cada
 *      vuelta. Con «v» deja de dar señal, aunque sigue funcionando; con «x» se
 *      queda bloqueada sin plazo. En los dos casos el aviso la nombra a ella,
 *      y la tarea inactiva sigue ejecutándose.
 *   5  pánico: como la 4, pero antes reconfigura el watchdog con
 *      esp_task_wdt_reconfigure() para que, al vencer, llame al gestor de
 *      pánico, que reinicia la placa. Solo existe si se activa en el menú de
 *      configuración.
 *
 * Las líneas de las teclas y las de «vigilada» llevan el instante en
 * milisegundos desde el arranque, t, en la misma escala que la marca de los
 * avisos del watchdog, E (...): así se puede medir el plazo en el monitor.
 */

#include "comun.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "sdkconfig.h"

#define PERIODO_VIGILADA_MS  1000

static volatile bool s_acaparar = false;
static volatile bool s_alimentar = true;
static volatile bool s_bloquear = false;
static TaskHandle_t s_vigilada;
static char s_variante;

/* Milisegundos desde el arranque, como la marca de las líneas del registro. */
static unsigned long instante(void)
{
    return (unsigned long)esp_log_timestamp();
}

static void tarea_acaparadora(void *argumento)
{
    (void)argumento;
    for (;;) {
        while (s_acaparar) {
            /* espera activa: la tarea no se bloquea y, con prioridad 5, no
             * deja ejecutarse a las de menos prioridad de su núcleo */
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void tarea_vigilada(void *argumento)
{
    (void)argumento;
    /* NULL: se suscribe la propia tarea. Desde ahora, si pasa el plazo sin
     * que llame a esp_task_wdt_reset(), el watchdog la nombra en su aviso. */
    const esp_err_t error = esp_task_wdt_add(NULL);
    if (error != ESP_OK) {
        printf("vigilada: esp_task_wdt_add() devuelve %s\n", esp_err_to_name(error));
    }
    TickType_t referencia = xTaskGetTickCount();
    bool daba_senal = true;
    for (;;) {
        trabajar_us(2000);  /* su trabajo de cada vuelta */
        const bool dar_senal = s_alimentar;
        if (dar_senal) {
            esp_task_wdt_reset();
            if (!daba_senal) {
                printf("vigilada: primera senal al watchdog tras la tecla (t = %lu ms)\n",
                       instante());
            }
        }
        daba_senal = dar_senal;
        if (s_bloquear) {
            printf("vigilada: se bloquea sin plazo; ya no da senal al watchdog (t = %lu ms)\n",
                   instante());
            ulTaskNotifyTake(pdTRUE, 0);  /* descarta un aviso que llegó antes de tiempo */
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            printf("vigilada: desbloqueada (t = %lu ms)\n", instante());
            referencia = xTaskGetTickCount();
        }
        xTaskDelayUntil(&referencia, pdMS_TO_TICKS(PERIODO_VIGILADA_MS));
    }
}

#if CONFIG_SEYTR_VARIANTE_PANICO
static void activar_panico(void)
{
    /* Se conserva el plazo y las tareas inactivas vigiladas de la
     * configuración, y solo se cambia lo que pasa al vencer. */
    uint32_t inactivas = 0;
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
    inactivas |= 1u << 0;
#endif
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
    inactivas |= 1u << 1;
#endif
    const esp_task_wdt_config_t configuracion = {
        .timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000,
        .idle_core_mask = inactivas,
        .trigger_panic = true,
    };
    const esp_err_t error = esp_task_wdt_reconfigure(&configuracion);
    printf("Watchdog de tareas reconfigurado para llamar al gestor de panico: %s\n",
           esp_err_to_name(error));
}
#endif

void vigilancia_crear(char variante)
{
    s_variante = variante;
    BaseType_t ok;
    if (variante == '3') {
        ok = xTaskCreatePinnedToCore(tarea_acaparadora, "acaparadora", PILA_TAREA, NULL, 5,
                                     NULL, NUCLEO_PRUEBA);
    } else {
#if CONFIG_SEYTR_VARIANTE_PANICO
        if (variante == '5') {
            activar_panico();
        }
#endif
        ok = xTaskCreatePinnedToCore(tarea_vigilada, "vigilada", PILA_TAREA, NULL, 3,
                                     &s_vigilada, NUCLEO_PRUEBA);
    }
    if (ok != pdPASS) {
        printf("No hay memoria para la tarea de la prueba.\n");
    }
}

bool vigilancia_tecla(char tecla)
{
    if (s_variante == '3' && tecla == 'b') {
        s_acaparar = !s_acaparar;
        if (s_acaparar) {
            printf("\nacaparadora deja de bloquearse (t = %lu ms). El aviso deberia salir"
                   " en unos %d s.\n", instante(), CONFIG_ESP_TASK_WDT_TIMEOUT_S);
        } else {
            printf("\nacaparadora vuelve a bloquearse (t = %lu ms).\n", instante());
        }
        return true;
    }
    if (s_variante != '3' && tecla == 'v') {
        s_alimentar = !s_alimentar;
        if (s_alimentar) {
            printf("\nTecla v (t = %lu ms): vigilada vuelve a dar senal al watchdog en su"
                   " siguiente vuelta.\n", instante());
        } else {
            printf("\nTecla v (t = %lu ms): vigilada deja de dar senal al watchdog, aunque"
                   " sigue funcionando.\n", instante());
        }
        return true;
    }
    if (s_variante == '4' && tecla == 'x') {
        printf("\nTecla x (t = %lu ms)\n", instante());
        if (!s_bloquear) {
            s_bloquear = true;  /* la tarea se bloquea en su siguiente vuelta */
        } else {
            s_bloquear = false;
            xTaskNotifyGive(s_vigilada);
        }
        return true;
    }
    return false;
}
