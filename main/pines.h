/* pines.h - los pines del proyecto, en un solo sitio.
 *
 * Cada número de pin se escribe aquí y en ningún otro fichero de código. La
 * tabla de pines del README tiene que decir lo mismo que este fichero.
 *
 * Solo los usa la variante 6, la salida modulada. Las demás variantes no
 * necesitan montaje.
 */
#pragma once

#include "driver/gpio.h"

/* Pin del LED externo, que gobierna la salida modulada.
 *
 * GPIO_NUM_NC significa «sin conectar»: el repositorio llega así a propósito,
 * porque el pin lo eliges tú. Sin LED, la variante 6 avisa al arrancar y no
 * hace nada más.
 *
 * Se elige como en U2-S1: con el esquema de tu placa y la tabla de pines de la
 * hoja de características del ESP32-S3, descartando los que sirven a la
 * memoria externa, los de arranque, los del USB y la consola y los que la
 * placa ya usa. Después escribe aquí su número, por ejemplo GPIO_NUM_xx,
 * compila y graba. */
#define PIN_LED                 GPIO_NUM_NC

/* Nivel lógico que enciende el LED. Lo decide el montaje, no el código:
 *   1 si el LED y su resistencia van entre el pin y masa;
 *   0 si van entre el pin y 3,3 V. Con 0, la salida modulada se invierte. */
#define LED_NIVEL_ENCENDIDO     1

/* Pin del pulsador, que cambia el modo del LED. Es opcional: con GPIO_NUM_NC,
 * la tecla «m» del monitor hace lo mismo, sin pasar por la interrupción. Se
 * elige como el del LED. */
#define PIN_PULSADOR            GPIO_NUM_NC

/* Nivel lógico que se lee con el pulsador apretado. Lo decide el montaje:
 *   0 si el pulsador va entre el pin y masa, con pull-up: es el de U2-S2;
 *   1 si va entre el pin y 3,3 V, con pull-down. */
#define PULSADOR_NIVEL_PULSADO  0
