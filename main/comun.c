/* comun.c - piezas que comparten las variantes del ejemplo. */

#include "comun.h"

#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_timer.h"

void trabajar_us(int64_t us)
{
    int64_t hecho = 0;
    int64_t previo = esp_timer_get_time();
    while (hecho < us) {
        const int64_t ahora = esp_timer_get_time();
        const int64_t hueco = ahora - previo;
        if (hueco <= HUECO_US) {
            hecho += hueco;  /* tiempo propio */
        }                    /* un hueco mayor es tiempo de otra tarea */
        previo = ahora;
    }
}

const char *nombre_estado(eTaskState estado)
{
    switch (estado) {
    case eRunning:   return "en ejecucion";
    case eReady:     return "lista";
    case eBlocked:   return "bloqueada";
    case eSuspended: return "suspendida";
    case eDeleted:   return "borrada";
    default:         return "no valida";
    }
}

void escribir_memoria(void)
{
    /* MALLOC_CAP_DEFAULT es la memoria que da malloc(). La mínima desde el
     * arranque dice cuánto se ha llegado a usar; el mayor bloque libre, si la
     * memoria está fragmentada. */
    printf("Memoria libre: %u bytes; minima desde el arranque: %u; mayor bloque libre: %u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

/* Unos 40 bytes por tarea, dice la documentación de vTaskList(); el programa
 * tiene menos de veinte. */
static char s_tabla[2048];

void informe_completo(void)
{
    printf("\n--- Lista de tareas (vTaskList) ---\n");
    printf("Nombre\t\tEstado\tPrio\tPila\tNum\tNucleo\n");
    vTaskList(s_tabla);
    printf("%s", s_tabla);
    printf("Estado: X en ejecucion (la tarea que pide la lista), R lista, B bloqueada,\n"
           "S suspendida o bloqueada sin plazo esperando una notificacion, D borrada.\n");
    printf("Pila: lo minimo que le ha quedado libre, en bytes. Nucleo: -1 sin afinidad.\n");

    printf("\n--- Reparto del procesador desde el arranque (vTaskGetRunTimeStats) ---\n");
    printf("Nombre\t\tTiempo (us)\tPorcentaje\n");
    vTaskGetRunTimeStats(s_tabla);
    printf("%s", s_tabla);
    printf("Con dos nucleos, los porcentajes suman hasta 200 %%.\n\n");

    escribir_memoria();
}
