/* comun.h - piezas que comparten las variantes del ejemplo.
 *
 * Las tareas de la prueba se ejecutan en el núcleo 0 del procesador. Las que
 * observan (el informe y el teclado) van fijadas al núcleo 1 y con prioridad
 * más alta, para que observar no altere lo que se observa y para que sigan
 * respondiendo aunque el núcleo 0 quede ocupado, como en la variante 3.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define NUCLEO_PRUEBA       0
#define NUCLEO_OBSERVACION  1

/* Por encima de todas las tareas de la prueba y por debajo de las del
 * sistema que importan (la de esp_timer tiene 22). */
#define PRIORIDAD_INFORME   10
#define PRIORIDAD_TECLADO   9

#define PILA_TAREA          3072  /* en bytes, como pide ESP-IDF */
#define PILA_INFORME        4096  /* vTaskList() usa sprintf(), que gasta pila */

/* Simula un trabajo de cálculo de «us» microsegundos de procesador, con una
 * espera activa que no se bloquea. Cuenta solo el tiempo en que la tarea se
 * ejecuta: no cuenta los huecos de más de HUECO_US, que son el tiempo en que
 * se ejecutó otra cosa. Es la de SEyTR_planificacion_de_tareas. */
#define HUECO_US 50
void trabajar_us(int64_t us);

/* Informe completo: la lista de tareas, el reparto del procesador desde el
 * arranque y la memoria libre. Lo escribe la tarea del informe, al pulsar «i»
 * y en el informe periódico de la variante 1. Usa un búfer estático: solo
 * puede llamarlo esa tarea. */
void informe_completo(void);

/* Una línea con la memoria libre ahora, la mínima desde el arranque y el
 * mayor bloque libre. */
void escribir_memoria(void);

/* Texto del estado de una tarea, tal como lo da eTaskGetState(). */
const char *nombre_estado(eTaskState estado);

/* Variante 1: estado, pila y memoria. */
void inspeccion_crear(void);
void inspeccion_informe(void);
bool inspeccion_tecla(char tecla);

/* Variante 2: carga del procesador con la tarea inactiva. */
void carga_crear(void);
void carga_informe(void);
bool carga_tecla(char tecla);

/* Variantes 3, 4 y 5: los avisos del watchdog de tareas. */
void vigilancia_crear(char variante);
bool vigilancia_tecla(char tecla);

/* Variante 6: salida modulada con LEDC. */
void salida_crear(void);
bool salida_tecla(char tecla);
