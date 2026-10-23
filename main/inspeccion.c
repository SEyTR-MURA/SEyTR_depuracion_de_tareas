/* inspeccion.c - variante 1: estado, pila y memoria de cada tarea.
 *
 * Crea tres tareas de prueba en el núcleo 0, cada una en un estado distinto:
 *   trabajo     periódica, cada 100 ms prepara un texto en un vector local
 *               de 512 bytes, que ocupa pila;
 *   dormida     espera 60 s en cada vuelta: casi siempre está bloqueada;
 *   suspendida  se suspende a sí misma nada más empezar.
 * Cada 5 s, la tarea del informe escribe el informe completo y, de cada tarea
 * del programa, su estado con eTaskGetState() y su marca de agua con
 * uxTaskGetStackHighWaterMark(). Para «trabajo» usa además vTaskGetInfo().
 *
 * Las teclas «r» y «l» reservan memoria que no se libera y la liberan, para
 * ver cómo cambian la memoria libre y la mínima.
 */

#include "comun.h"

#include <stdio.h>
#include <stdlib.h>

#include "sdkconfig.h"

#define PERIODO_TRABAJO_MS  100
#define RESERVA_BYTES       4096
#define MAX_RESERVAS        32

static TaskHandle_t s_trabajo;
static TaskHandle_t s_dormida;
static TaskHandle_t s_suspendida;

static void *s_reservas[MAX_RESERVAS];
static int s_n_reservas = 0;

static void tarea_trabajo(void *argumento)
{
    (void)argumento;
    uint32_t vuelta = 0;
    TickType_t referencia = xTaskGetTickCount();
    for (;;) {
        /* Un vector local vive en la pila de la tarea: la marca de agua lo
         * refleja. snprintf() también gasta pila. */
        char texto[512];
        snprintf(texto, sizeof texto, "vuelta %lu", (unsigned long)vuelta++);
        trabajar_us(1000);
        xTaskDelayUntil(&referencia, pdMS_TO_TICKS(PERIODO_TRABAJO_MS));
    }
}

static void tarea_dormida(void *argumento)
{
    (void)argumento;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

static void tarea_suspendida(void *argumento)
{
    (void)argumento;
    vTaskSuspend(NULL);  /* nadie la reanuda */
    for (;;) {
        vTaskDelay(portMAX_DELAY);
    }
}

void inspeccion_crear(void)
{
    bool ok = xTaskCreatePinnedToCore(tarea_trabajo, "trabajo", PILA_TAREA, NULL, 3,
                                      &s_trabajo, NUCLEO_PRUEBA) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(tarea_dormida, "dormida", PILA_TAREA, NULL, 2,
                                       &s_dormida, NUCLEO_PRUEBA) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(tarea_suspendida, "suspendida", PILA_TAREA, NULL, 2,
                                       &s_suspendida, NUCLEO_PRUEBA) == pdPASS;
    if (!ok) {
        printf("No hay memoria para las tareas de la variante 1.\n");
    }
}

static void escribir_tarea(TaskHandle_t tarea, unsigned pila_bytes)
{
    if (tarea == NULL) {
        return;
    }
    const unsigned libre = (unsigned)uxTaskGetStackHighWaterMark(tarea);
    printf("  %-12s %-13s pila de %u bytes: minimo libre %u, usados como mucho %u\n",
           pcTaskGetName(tarea), nombre_estado(eTaskGetState(tarea)),
           pila_bytes, libre, pila_bytes - libre);
}

void inspeccion_informe(void)
{
    informe_completo();

    printf("\n--- Cada tarea del programa (eTaskGetState y uxTaskGetStackHighWaterMark) ---\n");
    escribir_tarea(s_trabajo, PILA_TAREA);
    escribir_tarea(s_dormida, PILA_TAREA);
    escribir_tarea(s_suspendida, PILA_TAREA);
    escribir_tarea(xTaskGetHandle("teclado"), PILA_TAREA);
    escribir_tarea(xTaskGetCurrentTaskHandle(), PILA_INFORME);  /* el informe */

    /* vTaskGetInfo() rellena de una vez la estructura que usan vTaskList() y
     * vTaskGetRunTimeStats(). Con eInvalid calcula el estado; con pdTRUE, la
     * marca de agua, que tarda porque recorre la pila. */
    if (s_trabajo != NULL) {
        TaskStatus_t datos;
        vTaskGetInfo(s_trabajo, &datos, pdTRUE, eInvalid);
        printf("\n--- vTaskGetInfo de \"%s\" ---\n", datos.pcTaskName);
        printf("  numero %u, prioridad %u, estado %s, pila minima libre %u bytes,\n"
               "  tiempo de ejecucion acumulado %lu us\n",
               (unsigned)datos.xTaskNumber, (unsigned)datos.uxCurrentPriority,
               nombre_estado(datos.eCurrentState), (unsigned)datos.usStackHighWaterMark,
               (unsigned long)datos.ulRunTimeCounter);
#if CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID
        /* xCoreID solo existe con la opción que añade la columna del núcleo. */
        printf("  nucleo %d\n", (int)datos.xCoreID);
#endif
    }
    printf("Reservas de %u bytes sin liberar: %d\n\n", RESERVA_BYTES, s_n_reservas);
}

bool inspeccion_tecla(char tecla)
{
    switch (tecla) {
    case 'r':
        if (s_n_reservas >= MAX_RESERVAS) {
            printf("\nYa hay %d reservas: libera con l.\n", s_n_reservas);
        } else {
            void *bloque = malloc(RESERVA_BYTES);
            if (bloque == NULL) {
                printf("\nmalloc() devuelve NULL: no hay un bloque libre de %u bytes.\n",
                       RESERVA_BYTES);
            } else {
                s_reservas[s_n_reservas++] = bloque;
                printf("\nReservados %u bytes que no se liberan (%d reservas).\n",
                       RESERVA_BYTES, s_n_reservas);
                escribir_memoria();
            }
        }
        return true;
    case 'l':
        for (int i = 0; i < s_n_reservas; i++) {
            free(s_reservas[i]);
        }
        printf("\nLiberadas %d reservas.\n", s_n_reservas);
        s_n_reservas = 0;
        escribir_memoria();
        return true;
    default:
        return false;
    }
}
