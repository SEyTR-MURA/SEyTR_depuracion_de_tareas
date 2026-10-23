/* depuracion_de_tareas.c - SEyTR, Unidad 3, sesión U3-S6.
 *
 * Qué hace este programa
 * ----------------------
 * Pone en marcha, según la variante que se elija desde el monitor, unas
 * tareas de prueba y las herramientas con que se depura un sistema de tareas:
 * el estado, la pila y la memoria de cada tarea, la lista de tareas y el
 * reparto del procesador, la carga medida con la tarea inactiva y los avisos
 * del watchdog de tareas. La última variante prepara la salida modulada del
 * Taller 2.
 *
 *   1 inspección · 2 carga · 3 aviso del watchdog · 4 tarea vigilada
 *   5 pánico (desactivada por omisión) · 6 salida modulada
 * Las teclas se listan al arrancar y con «h». La tecla «i» escribe en
 * cualquier variante la lista de tareas, el reparto del procesador y la
 * memoria libre.
 *
 * Qué hardware necesita
 * ---------------------
 * La placa del curso, conectada por su conector USB-a-UART, que es por donde
 * llegan las teclas. Las variantes 1 a 5 no necesitan montaje. La 6 necesita
 * un LED con su resistencia y, si se quiere, un pulsador, como en U2-S2; sus
 * pines se eligen en main/pines.h.
 *
 * Cómo está organizado
 * --------------------
 * Las tareas de la prueba van en el núcleo 0. La tarea del informe y la del
 * teclado van fijadas al núcleo 1, con prioridad más alta que cualquier tarea
 * de la prueba: así siguen funcionando aunque el núcleo 0 quede ocupado.
 *
 * Cambiar de variante reinicia la placa. La variante elegida se guarda en
 * variables marcadas con __NOINIT_ATTR, que conservan su valor tras un
 * reinicio por programa, y se comprueba con un número mágico, como en
 * SEyTR_planificacion_de_tareas. Tras un reinicio que no sea por programa,
 * como el del gestor de pánico de la variante 5, el programa arranca en la 1.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "comun.h"

static const char *TAG = "depuracion";

#if CONFIG_SEYTR_VARIANTE_PANICO
#define PANICO_PERMITIDO true
#else
#define PANICO_PERMITIDO false
#endif

#define MAGIA 0x53455452u  /* "SETR": dice que lo guardado es válido */

static __NOINIT_ATTR uint32_t s_guardado_magia;
static __NOINIT_ATTR uint32_t s_guardado_variante;

static char s_variante = '1';
static TaskHandle_t s_informe;

static bool es_variante(char v)
{
    return v >= '1' && v <= '6';
}

static const char *nombre_variante(char v)
{
    switch (v) {
    case '1': return "inspeccion: estado, pila y memoria de cada tarea";
    case '2': return "carga: capacidad libre medida con la tarea inactiva";
    case '3': return "aviso del watchdog: una tarea deja de bloquearse";
    case '4': return "tarea vigilada: una tarea propia suscrita al watchdog";
    case '5': return "panico: el watchdog llama al gestor de panico y reinicia";
    case '6': return "salida modulada: el LED con LEDC";
    }
    return "?";
}

static const char *nombre_reinicio(esp_reset_reason_t motivo)
{
    switch (motivo) {
    case ESP_RST_POWERON:  return "encendido";
    case ESP_RST_SW:       return "por programa";
    case ESP_RST_PANIC:    return "gestor de panico";
    case ESP_RST_INT_WDT:  return "watchdog de interrupciones";
    case ESP_RST_TASK_WDT: return "watchdog de tareas";
    case ESP_RST_WDT:      return "otro watchdog";
    case ESP_RST_BROWNOUT: return "caida de tension";
    default:               return "otro";
    }
}

static void cargar_variante(void)
{
    if (esp_reset_reason() == ESP_RST_SW && s_guardado_magia == MAGIA
        && es_variante((char)s_guardado_variante)) {
        s_variante = (char)s_guardado_variante;
    } else {
        s_variante = '1';
    }
}

static void cambiar_variante(char nueva)
{
#if !CONFIG_SEYTR_VARIANTE_PANICO
    if (nueva == '5') {
        printf("\nLa variante 5 reinicia la placa por el gestor de panico y viene desactivada.\n"
               "Activala en el menu de configuracion: SEyTR - Depuracion de tareas.\n");
        return;
    }
#endif
    s_guardado_magia = MAGIA;
    s_guardado_variante = (uint32_t)nueva;
    printf("\nCambio a la variante %c: %s. Se reinicia la placa.\n", nueva, nombre_variante(nueva));
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));  /* que salga el mensaje */
    esp_restart();
}

static void mostrar_ayuda(void)
{
    printf("\n=== SEyTR - depuracion de tareas ===\n");
    printf("Variante actual: %c, %s\n", s_variante, nombre_variante(s_variante));
    printf("Teclas (sin Intro). Cambiar de variante reinicia la placa.\n");
    printf("  1 inspeccion   2 carga   3 aviso del watchdog   4 tarea vigilada\n");
    printf("  5 panico%s   6 salida modulada\n",
           PANICO_PERMITIDO ? "" : " (desactivada)");
    printf("  i  lista de tareas, reparto del procesador y memoria libre\n");
    switch (s_variante) {
    case '1': printf("  r  reserva 4 KB que no se liberan    l  libera lo reservado\n"); break;
    case '2': printf("  c  cambia la carga del nucleo 0: 0, 25, 50 o 75 %%\n"); break;
    case '3': printf("  b  la tarea acaparadora deja de bloquearse, o vuelve a hacerlo\n"); break;
    case '4': printf("  v  la tarea vigilada deja de dar senal al watchdog, o vuelve\n");
              printf("  x  la tarea vigilada se bloquea sin plazo, o se desbloquea\n"); break;
    case '5': printf("  v  la tarea vigilada deja de dar senal al watchdog: reinicia\n"); break;
    case '6': printf("  m  modo siguiente, como el pulsador   f  frecuencia   r  resolucion\n"); break;
    }
    printf("  h  esta ayuda\n");
    printf("Tick a %d Hz. Watchdog de tareas: %d s.\n\n", configTICK_RATE_HZ,
           CONFIG_ESP_TASK_WDT_TIMEOUT_S);
}

/* Lee una tecla, si ha llegado alguna, y la atiende. No espera: sin el
 * controlador de la UART instalado, read() sobre la consola devuelve -1
 * enseguida si no ha llegado nada. */
static void atender_teclado(void)
{
    char tecla;
    if (read(STDIN_FILENO, &tecla, 1) != 1) {
        return;
    }
    if (es_variante(tecla)) {
        if (tecla != s_variante) {
            cambiar_variante(tecla);
        }
        return;
    }
    bool atendida = false;
    switch (s_variante) {
    case '1': atendida = inspeccion_tecla(tecla); break;
    case '2': atendida = carga_tecla(tecla); break;
    case '3': case '4': case '5': atendida = vigilancia_tecla(tecla); break;
    case '6': atendida = salida_tecla(tecla); break;
    }
    if (atendida) {
        return;
    }
    switch (tecla) {
    case 'i':
        xTaskNotifyGive(s_informe);  /* lo escribe la tarea del informe */
        break;
    case 'h': case '?':
        mostrar_ayuda();
        break;
    default:
        break;
    }
}

#define ESPERA_ANTES_DE_CREAR_MS  200

static void tarea_teclado(void *argumento)
{
    (void)argumento;
    /* Espera a que termine la tarea main antes de crear las tareas de la
     * prueba, como en SEyTR_planificacion_de_tareas: si una tarea que no se
     * bloquea la expulsara a mitad de una línea, main se quedaría con la
     * consola tomada. */
    vTaskDelay(pdMS_TO_TICKS(ESPERA_ANTES_DE_CREAR_MS));
    switch (s_variante) {
    case '1': inspeccion_crear(); break;
    case '2': carga_crear(); break;
    case '3': case '4': case '5': vigilancia_crear(s_variante); break;
    case '6': salida_crear(); break;
    }
    for (;;) {
        atender_teclado();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void tarea_informe(void *argumento)
{
    (void)argumento;
    /* Informe periódico en las variantes 1 y 2; en las demás, solo al pulsar
     * «i», que llega como notificación. */
    TickType_t periodo = portMAX_DELAY;
    if (s_variante == '1') {
        periodo = pdMS_TO_TICKS(5000);
    } else if (s_variante == '2') {
        periodo = pdMS_TO_TICKS(2000);
    }
    for (;;) {
        const bool pedido = ulTaskNotifyTake(pdTRUE, periodo) > 0;
        if (pedido) {
            informe_completo();
        } else if (s_variante == '1') {
            inspeccion_informe();
        } else if (s_variante == '2') {
            carga_informe();
        }
    }
}

void app_main(void)
{
    cargar_variante();

    const esp_reset_reason_t motivo = esp_reset_reason();
    ESP_LOGI(TAG, "SEyTR - depuracion de tareas. Ultimo reinicio: %s", nombre_reinicio(motivo));
    mostrar_ayuda();

    bool ok = xTaskCreatePinnedToCore(tarea_informe, "informe", PILA_INFORME, NULL,
                                      PRIORIDAD_INFORME, &s_informe, NUCLEO_OBSERVACION) == pdPASS;
    ok = ok && xTaskCreatePinnedToCore(tarea_teclado, "teclado", PILA_TAREA, NULL,
                                       PRIORIDAD_TECLADO, NULL, NUCLEO_OBSERVACION) == pdPASS;
    if (!ok) {
        ESP_LOGE(TAG, "no hay memoria para las tareas de observacion");
    }
    /* app_main termina aquí; las tareas siguen. */
}
