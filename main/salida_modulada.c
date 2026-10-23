/* salida_modulada.c - variante 6: el LED con una salida modulada de LEDC.
 *
 * LEDC es el periférico de modulación por ancho de pulso del ESP32-S3. Un
 * temporizador fija la frecuencia y la resolución, y un canal lo saca por un
 * pin con un ciclo de trabajo. En el ESP32-S3 solo existe el modo de baja
 * velocidad.
 *
 * El LED tiene cinco modos, que se recorren en orden: apagado, 10 %, 50 %,
 * 100 % y transición, en la que el brillo sube y baja solo, por hardware.
 * Cada pulsación pasa al modo siguiente. Como en U2-S2, la rutina de
 * interrupción del pulsador no toca el LED: anota el instante y avisa a la
 * tarea «led» por una cola. La tarea aplica el antirrebote y cambia el ciclo
 * de trabajo con ledc_set_duty_and_update(). La tarea «led» es la única que
 * usa LEDC; las teclas del monitor también le llegan por la cola.
 *
 * Teclas: «m» pasa al modo siguiente, como el pulsador pero sin pasar por la
 * interrupción; «f» cambia la frecuencia y «r» la resolución, y el driver
 * rechaza las combinaciones que el temporizador no puede dar.
 */

#include "comun.h"

#include <inttypes.h>
#include <stdio.h>

#include "freertos/queue.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "soc/soc_caps.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "pines.h"
#include "pulsador.h"

#define MODO_LEDC        LEDC_LOW_SPEED_MODE  /* el único del ESP32-S3 */
#define TEMPORIZADOR     LEDC_TIMER_0
#define CANAL            LEDC_CHANNEL_0
#define TRANSICION_MS    1500
#define GUARDA_US        ((int64_t)CONFIG_SEYTR_GUARDA_MS * 1000)

static const uint32_t FRECUENCIAS[] = { 1000, 5000, 20000 };   /* Hz */
static const uint32_t RESOLUCIONES[] = { 8, 10, 13, 14 };      /* bits */
#define N_FRECUENCIAS   (sizeof FRECUENCIAS / sizeof FRECUENCIAS[0])
#define N_RESOLUCIONES  (sizeof RESOLUCIONES / sizeof RESOLUCIONES[0])

typedef enum { APAGADO, BAJO, MEDIO, ALTO, TRANSICION, N_MODOS } modo_t;
static const int PORCENTAJE[] = { 0, 10, 50, 100 };

typedef enum { PULSACION, TECLA_MODO, TECLA_FRECUENCIA, TECLA_RESOLUCION, FIN_TRANSICION } tipo_t;

typedef struct {
    tipo_t   tipo;
    int64_t  us;     /* PULSACION: instante del flanco */
    uint32_t ciclo;  /* FIN_TRANSICION: ciclo de trabajo al terminar */
} orden_t;

static QueueHandle_t s_ordenes;
static pulsador_t s_pulsador;

/* Rutina de interrupción del pulsador: anota el instante y avisa a la tarea.
 * Nada más. */
static void IRAM_ATTR isr_pulsador(void *argumento)
{
    (void)argumento;
    const orden_t orden = { .tipo = PULSACION, .us = esp_timer_get_time(), .ciclo = 0 };
    BaseType_t despertada = pdFALSE;
    xQueueSendFromISR(s_ordenes, &orden, &despertada);
    portYIELD_FROM_ISR(despertada);
}

/* LEDC llama a esta función al terminar cada transición, desde su propia
 * rutina de interrupción. Tampoco toca el LED: avisa a la tarea. */
static bool IRAM_ATTR fin_de_transicion(const ledc_cb_param_t *parametro, void *argumento)
{
    (void)argumento;
    BaseType_t despertada = pdFALSE;
    if (parametro->event == LEDC_FADE_END_EVT) {
        const orden_t orden = { .tipo = FIN_TRANSICION, .us = 0, .ciclo = parametro->duty };
        xQueueSendFromISR(s_ordenes, &orden, &despertada);
    }
    return despertada == pdTRUE;
}

/* Estado de la salida. Solo lo toca la tarea «led». */
static int s_frecuencia = 1;   /* índice en FRECUENCIAS: 5 kHz */
static int s_resolucion = 2;   /* índice en RESOLUCIONES: 13 bits */
static int s_frecuencia_pedida = 1;  /* lo último que se ha pedido con «f» y «r»: */
static int s_resolucion_pedida = 2;  /* así se recorren también las rechazadas */
static modo_t s_modo = APAGADO;
static uint32_t s_destino = 0;       /* ciclo de trabajo al que va la transición */

/* Ciclo de trabajo del 100 %. La guía de LEDC admite valores de 0 a
 * 2^resolución, salvo con la resolución máxima del temporizador, 14 bits en
 * el ESP32-S3: con ella, 2^14 desborda el contador interno, y el máximo es
 * 2^14 - 1. */
static uint32_t maximo(void)
{
    const uint32_t bits = RESOLUCIONES[s_resolucion];
    return (bits >= SOC_LEDC_TIMER_BIT_WIDTH) ? (1u << bits) - 1u : 1u << bits;
}

static esp_err_t configurar_temporizador(int frecuencia, int resolucion)
{
    const ledc_timer_config_t temporizador = {
        .speed_mode = MODO_LEDC,
        .duty_resolution = (ledc_timer_bit_t)RESOLUCIONES[resolucion],
        .timer_num = TEMPORIZADOR,
        .freq_hz = FRECUENCIAS[frecuencia],
        .clk_cfg = LEDC_AUTO_CLK,  /* el driver elige un reloj que la pueda dar */
    };
    return ledc_timer_config(&temporizador);
}

static void empezar_transicion(uint32_t destino)
{
    s_destino = destino;
    ledc_set_fade_time_and_start(MODO_LEDC, CANAL, destino, TRANSICION_MS, LEDC_FADE_NO_WAIT);
}

static void aplicar_modo(void)
{
    /* ledc_fade_stop() detiene una transición empezada sin esperar; si no
     * hay ninguna, no hace nada. */
    ledc_fade_stop(MODO_LEDC, CANAL);
    if (s_modo == TRANSICION) {
        /* Desde el 100 %, con la resolución de ahora: la primera es una
         * bajada. */
        ledc_set_duty_and_update(MODO_LEDC, CANAL, maximo(), 0);
        empezar_transicion(0);
        printf("LED: transicion por hardware, de 100 a 0 %% y vuelta, %d ms cada una\n",
               TRANSICION_MS);
        return;
    }
    const uint32_t ciclo = (uint32_t)((uint64_t)maximo() * (uint32_t)PORCENTAJE[s_modo] / 100u);
    ledc_set_duty_and_update(MODO_LEDC, CANAL, ciclo, 0);
    printf("LED: %d %%, ciclo de trabajo %" PRIu32 " de %" PRIu32 "\n",
           PORCENTAJE[s_modo], ciclo, (uint32_t)(1u << RESOLUCIONES[s_resolucion]));
}

static void cambiar_temporizador(int frecuencia, int resolucion)
{
    ledc_fade_stop(MODO_LEDC, CANAL);
    const uint64_t reloj = (uint64_t)FRECUENCIAS[frecuencia] << RESOLUCIONES[resolucion];
    printf("Temporizador: %" PRIu32 " Hz con %" PRIu32 " bits pide un reloj de al menos %llu Hz\n",
           FRECUENCIAS[frecuencia], RESOLUCIONES[resolucion], (unsigned long long)reloj);
    /* Si el driver rechaza la combinación, lo hace antes de tocar el
     * temporizador (ledc.c), que sigue como estaba. */
    if (configurar_temporizador(frecuencia, resolucion) == ESP_OK) {
        s_frecuencia = frecuencia;
        s_resolucion = resolucion;
        printf("Aceptado: %" PRIu32 " Hz, %" PRIu32 " bits (%" PRIu32 " niveles)\n",
               FRECUENCIAS[s_frecuencia], RESOLUCIONES[s_resolucion],
               (uint32_t)(1u << RESOLUCIONES[s_resolucion]));
    } else {
        printf("Rechazado: se mantiene %" PRIu32 " Hz con %" PRIu32 " bits\n",
               FRECUENCIAS[s_frecuencia], RESOLUCIONES[s_resolucion]);
    }
    aplicar_modo();  /* el ciclo de trabajo depende de la resolución */
}

/* Antirrebote de U2-S2: tiempo de guarda y confirmación del nivel. Un flanco
 * dentro de la guarda es rebote. Fuera de ella, la tarea espera a que pase el
 * rebote y comprueba que el pin sigue pulsado: así no cuenta los rebotes al
 * soltar, que pueden llegar con la guarda ya vencida. Mientras espera, los
 * flancos que llegan quedan en la cola y se descartan después por la guarda,
 * que es más larga que la confirmación. */
static bool es_pulsacion(int64_t flanco_us, int64_t *ultima_us)
{
    if (flanco_us - *ultima_us < GUARDA_US) {
        return false;
    }
    *ultima_us = flanco_us;
    vTaskDelay(pdMS_TO_TICKS(CONFIG_SEYTR_CONFIRMACION_MS));
    return pulsador_pulsado(&s_pulsador);
}

static void tarea_led(void *argumento)
{
    (void)argumento;
    int64_t ultima_us = INT64_MIN / 2;  /* el primer flanco queda fuera de la guarda */
    uint32_t descartados = 0;
    orden_t orden;
    for (;;) {
        xQueueReceive(s_ordenes, &orden, portMAX_DELAY);
        switch (orden.tipo) {
        case PULSACION:
            if (!es_pulsacion(orden.us, &ultima_us)) {
                descartados++;
                break;
            }
            printf("\nPulsacion (flancos descartados hasta ahora: %" PRIu32 ")\n", descartados);
            s_modo = (modo_t)((s_modo + 1) % N_MODOS);
            aplicar_modo();
            break;
        case TECLA_MODO:
            s_modo = (modo_t)((s_modo + 1) % N_MODOS);
            aplicar_modo();
            break;
        case TECLA_FRECUENCIA:
            s_frecuencia_pedida = (s_frecuencia_pedida + 1) % (int)N_FRECUENCIAS;
            cambiar_temporizador(s_frecuencia_pedida, s_resolucion);
            break;
        case TECLA_RESOLUCION:
            s_resolucion_pedida = (s_resolucion_pedida + 1) % (int)N_RESOLUCIONES;
            cambiar_temporizador(s_frecuencia, s_resolucion_pedida);
            break;
        case FIN_TRANSICION:
            /* Un aviso atrasado, de una transición que se cortó, se ignora: el
             * ciclo de trabajo no está en el destino de la transición en curso. */
            if (s_modo == TRANSICION && orden.ciclo == s_destino) {
                empezar_transicion(s_destino == 0 ? maximo() : 0);
            }
            break;
        }
    }
}

void salida_crear(void)
{
    if (PIN_LED == GPIO_NUM_NC) {
        printf("La variante 6 necesita el LED: elige su pin en main/pines.h, compila y graba.\n");
        return;
    }
    s_ordenes = xQueueCreate(8, sizeof(orden_t));
    if (s_ordenes == NULL) {
        printf("No hay memoria para la cola.\n");
        return;
    }

    ESP_ERROR_CHECK(configurar_temporizador(s_frecuencia, s_resolucion));
    const ledc_channel_config_t canal = {
        .gpio_num = PIN_LED,
        .speed_mode = MODO_LEDC,
        .channel = CANAL,
        .timer_sel = TEMPORIZADOR,
        .duty = 0,
        .hpoint = 0,
        .flags.output_invert = (LED_NIVEL_ENCENDIDO == 0),
    };
    ESP_ERROR_CHECK(ledc_channel_config(&canal));
    /* Instala la rutina de interrupción de LEDC que usan las transiciones y
     * las funciones seguras entre tareas, como ledc_set_duty_and_update(). */
    ESP_ERROR_CHECK(ledc_fade_func_install(0));
    ledc_cbs_t avisos = { .fade_cb = fin_de_transicion };
    ESP_ERROR_CHECK(ledc_cb_register(MODO_LEDC, CANAL, &avisos, NULL));
    printf("LEDC: %" PRIu32 " Hz, %" PRIu32 " bits, LED en el GPIO %d\n",
           FRECUENCIAS[s_frecuencia], RESOLUCIONES[s_resolucion], (int)PIN_LED);

    if (xTaskCreatePinnedToCore(tarea_led, "led", PILA_TAREA, NULL, 4, NULL,
                                NUCLEO_PRUEBA) != pdPASS) {
        printf("No hay memoria para la tarea del LED.\n");
        return;
    }

    if (PIN_PULSADOR == GPIO_NUM_NC) {
        printf("Sin pulsador: la tecla m cambia de modo.\n");
        return;
    }
    if (pulsador_iniciar(&s_pulsador, PIN_PULSADOR, PULSADOR_NIVEL_PULSADO) == ESP_OK) {
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_PULSADOR, isr_pulsador, NULL));
    }
}

static void enviar(tipo_t tipo)
{
    const orden_t orden = { .tipo = tipo, .us = 0, .ciclo = 0 };
    if (xQueueSend(s_ordenes, &orden, 0) != pdTRUE) {
        printf("\nLa cola esta llena: se pierde la orden.\n");
    }
}

bool salida_tecla(char tecla)
{
    if (s_ordenes == NULL) {
        return false;
    }
    switch (tecla) {
    case 'm': enviar(TECLA_MODO);       return true;
    case 'f': enviar(TECLA_FRECUENCIA); return true;
    case 'r': enviar(TECLA_RESOLUCION); return true;
    default:  return false;
    }
}
