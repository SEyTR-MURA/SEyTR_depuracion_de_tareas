# SEyTR · Depuración de tareas

Proyecto de ejemplo de **Sistemas Empotrados y de Tiempo Real**. Es el de la
sesión **U3-S6**, depuración de tareas y preparación del Taller 2. Reúne las
herramientas con que se pregunta al planificador cómo va un sistema de tareas:
el estado, la pila y la memoria de cada tarea, la lista de tareas y el reparto
del procesador, la carga medida con la tarea inactiva y los avisos del
*watchdog* de tareas. La última variante gobierna un LED con una salida
modulada de LEDC, que se practica en el Taller 2.

No hace falta cuenta ni permisos para clonarlo: el repositorio es público.

## Qué necesitas

El entorno instalado y verificado con la guía de instalación de la asignatura,
y la placa con su cable USB de datos, conectado al conector **USB-a-UART** de
la placa, que es por donde llegan las teclas al programa.

Las variantes 1 a 5 **no necesitan montaje**: todo se ve en el monitor. La
variante 6 necesita el montaje de U2-S2: un LED con su resistencia y, si
quieres, un pulsador. Sus pines se eligen en `main/pines.h`.

## Cómo obtenerlo

Clona el repositorio en una carpeta de tu equipo con una ruta **corta y sin
espacios ni tildes**, y abre en el editor la carpeta que contiene el
`CMakeLists.txt` de la raíz. Compila, graba y abre el monitor desde la
extensión de ESP-IDF, como en las sesiones anteriores. Si prefieres la línea
de órdenes, `idf.py build flash monitor` hace lo mismo.

## Cómo está organizado

Las tareas de la prueba van en el **núcleo 0**. La tarea del informe y la del
teclado van fijadas al **núcleo 1**, con prioridad más alta que cualquier tarea
de la prueba, para que observar no altere lo que se observa y para que sigan
respondiendo aunque el núcleo 0 quede ocupado, como en la variante 3.

El proyecto activa en `sdkconfig.defaults` las opciones de FreeRTOS que hacen
falta para listar las tareas y medir su tiempo de ejecución. Vienen
desactivadas en ESP-IDF, y en el menú de configuración están en
**Component config → FreeRTOS → Kernel**, rotuladas con el nombre de la macro
de FreeRTOS:

| Opción del menú | Qué hace |
| --- | --- |
| `configUSE_TRACE_FACILITY` | Guarda en cada tarea los datos con que se lista |
| `configUSE_STATS_FORMATTING_FUNCTIONS` | Incluye `vTaskList()` y `vTaskGetRunTimeStats()`, que escriben esos datos como tabla |
| `configGENERATE_RUN_TIME_STATS` | Cuenta el tiempo de ejecución de cada tarea; activa las otras dos |
| *Enable display of xCoreID in vTaskList* | Añade a `vTaskList()` la columna del núcleo |

Cuestan memoria y tiempo, así que en un producto se dejan desactivadas si no
hacen falta.

## Cómo se usa

Al arrancar, el programa escribe el motivo del último reinicio y la lista de
teclas, y empieza con la variante 1. Las teclas se pulsan sin Intro.
**Cambiar de variante reinicia la placa**, que arranca con la variante nueva.

| Tecla | Qué hace |
| --- | --- |
| `1` | Inspección: estado, pila y memoria de cada tarea |
| `2` | Carga: capacidad libre de cada núcleo medida con la tarea inactiva |
| `3` | Aviso del *watchdog*: una tarea deja de bloquearse |
| `4` | Tarea vigilada: una tarea propia suscrita al *watchdog* |
| `5` | Pánico: el *watchdog* llama al gestor de pánico y reinicia. Desactivada por omisión |
| `6` | Salida modulada: el LED con LEDC |
| `i` | En cualquier variante: lista de tareas, reparto del procesador y memoria libre |
| `r`, `l` | En la 1: reserva 4 KB que no se liberan; libera lo reservado |
| `c` | En la 2: cambia la carga del núcleo 0, entre 0, 25, 50 y 75 % |
| `b` | En la 3: la tarea `acaparadora` deja de bloquearse, o vuelve a hacerlo |
| `v` | En la 4 y la 5: la tarea `vigilada` deja de dar señal al *watchdog*, o vuelve |
| `x` | En la 4: la tarea `vigilada` se queda bloqueada sin plazo, o se desbloquea |
| `m` | En la 6: pasa al modo siguiente del LED, como el pulsador |
| `f`, `r` | En la 6: prueba la frecuencia siguiente con la resolución de ahora; prueba la resolución siguiente con la frecuencia de ahora |
| `h` | Vuelve a escribir la lista de teclas |

## Las variantes

### 1 · Inspección

Crea tres tareas en el núcleo 0, cada una en un estado: `trabajo`, periódica
cada 100 ms, que prepara un texto en un vector local de 512 bytes; `dormida`,
que espera 60 s en cada vuelta; y `suspendida`, que se suspende a sí misma.
Cada 5 s el informe escribe:

- la **lista de tareas** con `vTaskList()`: nombre, estado, prioridad, lo mínimo
  que le ha quedado libre de pila, número y núcleo;
- el **reparto del procesador** con `vTaskGetRunTimeStats()`: el tiempo que
  cada tarea ha estado en ejecución desde el arranque, en microsegundos, y el
  porcentaje;
- la **memoria libre** con `heap_caps_get_free_size()`, la mínima desde el
  arranque y el mayor bloque libre;
- de cada tarea del programa, su estado con `eTaskGetState()` y su marca de
  agua con `uxTaskGetStackHighWaterMark()`, en bytes, y lo más que ha usado de
  su pila;
- los datos de `trabajo` con `vTaskGetInfo()`, que rellena de una vez la
  estructura que usan las dos tablas.

La tecla `r` reserva 4 KB con `malloc()` y no los libera, como haría un
programa con una fuga de memoria; `l` los libera.

### 2 · Carga

Registra una **función de enganche** en la tarea inactiva de cada núcleo con
 `esp_register_freertos_idle_hook_for_cpu()`. Cada una cuenta sus llamadas y
devuelve `false`, para que la tarea inactiva la llame tan deprisa como pueda.
Si todas las funciones de enganche de un núcleo devolvieran `true`, la tarea
inactiva pararía el procesador hasta la siguiente interrupción y las llamaría
una sola vez por interrupción; la otra que hay, la del *watchdog* de tareas,
devuelve `true`. Por eso, mientras esta variante funciona, los núcleos no se
paran en reposo.

Antes de crear la tarea de carga, el programa cuenta durante 1 s las vueltas de
cada tarea inactiva con el núcleo casi libre. Esta medida se toma como referencia.
Cada 2 s, la **capacidad libre** de cada núcleo es lo contado en ese tiempo frente
a lo que daría la referencia, y la carga es el resto. El informe da al lado la
capacidad libre y la carga que salen del tiempo que las estadísticas de
FreeRTOS atribuyen a la tarea inactiva de cada núcleo en el mismo intervalo.
El primer informe no sale: solo toma la primera lectura.

La tarea `carga`, en el núcleo 0 con prioridad 4, se activa cada 100 ms y
calcula durante el porcentaje elegido de ese periodo. La tecla `c` lo cambia.

### 3 · Aviso del *watchdog*

Con la configuración de partida de ESP-IDF, el *watchdog* de tareas vigila la
tarea inactiva de cada núcleo, con un plazo de 5 s. Con la tecla `b`, la tarea
`acaparadora`, de prioridad 5 en el núcleo 0, deja de bloquearse: la tarea
inactiva de ese núcleo no llega a ejecutarse y, pasado el plazo, aparece el
aviso. La placa no se reinicia. Con `b` otra vez, la tarea vuelve a bloquearse
y los avisos cesan.

### 4 · Tarea vigilada

La tarea `vigilada` se suscribe al *watchdog* con `esp_task_wdt_add(NULL)` y
le da señal con `esp_task_wdt_reset()` en cada vuelta, una por segundo. Con
`v` deja de dar señal, aunque sigue funcionando y bloqueándose. Con `x` se
queda bloqueada sin plazo, como una tarea que espera algo que no llega. En los
dos casos el aviso la nombra a ella y la tarea inactiva sigue ejecutándose:
el *watchdog* vigila cada tarea suscrita por separado.

### 5 · Pánico

Como la 4, pero antes reconfigura el *watchdog* en ejecución con
`esp_task_wdt_reconfigure()` para que, al vencer, llame al **gestor de
pánico**. Con `v`, pasado el plazo, el gestor escribe la traza y reinicia la
placa, que arranca en la variante 1 y dice que el último reinicio fue por el
*watchdog* de tareas.

**Viene desactivada**, para que una tecla pulsada por error no reinicie la
placa. Se activa en el menú de configuración, en **SEyTR - Depuracion de
tareas**, con *Permitir la variante 5*.

### 6 · Salida modulada

LEDC es el periférico de **modulación por ancho de pulso** del ESP32-S3: un
temporizador fija la frecuencia y la resolución, y un canal saca la señal por
un pin con un ciclo de trabajo. En el ESP32-S3 solo existe el modo de baja
velocidad. El programa arranca con 5 kHz y 13 bits.

El LED tiene cinco modos, que se recorren en orden: apagado, 10 %, 50 %, 100 %
y **transición**, en la que el brillo sube y baja solo, en 1,5 s cada vez, por
hardware. Cada pulsación pasa al modo siguiente. Como en U2-S2, la rutina de
interrupción del pulsador no toca el LED: anota el instante y avisa a la tarea
`led` por una cola. La tarea aplica el antirrebote de U2-S2, con el tiempo de
guarda y la confirmación del nivel, y cambia el ciclo de trabajo con
`ledc_set_duty_and_update()`. La transición empieza desde el 100 %, así que lo
primero que se ve es una bajada. Al
terminar cada transición, LEDC llama desde su propia interrupción a una
función que tampoco toca el LED: avisa a la tarea, que empieza la siguiente.
La tarea `led` es la única que usa LEDC.

Sin pulsador, la tecla `m` hace lo mismo, pero sin pasar por la interrupción.
La tecla `f` recorre las frecuencias de 1, 5 y 20 kHz con la resolución de ahora,
y `r` recorre las resoluciones de 8, 10, 13 y 14 bits con la frecuencia de
ahora. El temporizador cuenta 2^resolución pasos en cada
periodo, así que necesita un reloj de al menos frecuencia × 2^resolución; el
más rápido de LEDC en el ESP32-S3 es el de 80 MHz. El driver rechaza las
combinaciones que no puede dar, con un error, y el programa mantiene la
anterior. El ciclo de trabajo va de 0 a 2^resolución, salvo con 14 bits, la
resolución máxima del ESP32-S3: con ella, la guía de LEDC advierte que 2^14
desborda el contador interno, y el programa usa 2^14 − 1 para el 100 %.

## Qué deberías ver

Para cada variante se da primero lo que se espera según el código y la
documentación de ESP-IDF, y después lo que se observó al probarla en una
placa. Si no coincide se explica a qué puede deberse la diferencia.

- **`1`.** *Esperado:* en la lista, `trabajo` sale bloqueada (`B`) casi
  siempre, `dormida` bloqueada y `suspendida` suspendida (`S`); `ipc0` e
  `ipc1`, que esperan una notificación sin plazo, también salen `S`. La tarea
  que escribe la lista sale `X`, en ejecución; una tarea que se esté
  ejecutando en el otro núcleo sale `R`, lista. Todas las tareas del programa
  están fijadas a un núcleo, así que la columna del núcleo da 0 o 1. La pila
  libre de `trabajo` es la menor de las tres, por su vector de 512 bytes. En
  el reparto, las tareas inactivas se llevan casi todo el tiempo, y entre
  todas las tareas los porcentajes suman cerca de 200 %, uno por núcleo. Cada
  `r` baja la memoria libre unos 4 KB y la mínima la sigue; `l` devuelve la
  libre, pero la mínima se queda donde llegó.

  *Observado:* todo como se espera, con un detalle: en el primer informe tras
  cada arranque, `trabajo` sale `R`. Ese informe cae en el mismo tick en que
  `trabajo` empieza una de sus vueltas, porque las dos tareas se pusieron en
  marcha con 200 ms de diferencia y sus periodos, 5 s y 100 ms, encajan; los
  informes siguientes se retrasan lo que tarda cada uno en escribirse y ya no
  coinciden. `ipc0` e `ipc1` tienen algo de tiempo desde el arranque, que no
  crece, y su porcentaje queda por debajo del 1 % desde el primer informe; de
  dónde sale ese tiempo no se ha comprobado.
- **`2`.** *Esperado:* sin carga, las dos medidas dan una capacidad libre
  cercana al 100 % en los dos núcleos. Con cada `c`, la del núcleo 0 baja de
  forma que la carga medida se acerca a la pedida, y las dos medidas
  coinciden. El primer informe después de cada cambio mezcla los dos niveles,
  porque su intervalo empezó antes de la tecla. El núcleo 1 apenas tiene
  carga, así que sigue cerca del 100 % libre con las dos medidas.

  *Observado:* en el núcleo 0, como se espera, con las dos medidas. En el
  núcleo 1, las estadísticas también, pero la medida con la función de
  enganche pasa del 100 % libre, con una carga negativa, y tanto más cuanto
  más carga tiene el núcleo 0. La medida por vueltas supone que cada vuelta de
  la tarea inactiva dura lo mismo que cuando se tomó la referencia; en el
  núcleo 1 no es así, porque lo que tarda cada vuelta depende de lo que hace
  el núcleo 0. La causa exacta no está determinada. No es la función de
  enganche del *watchdog* de tareas, que toma un cerrojo común a los dos
  núcleos: sin vigilar las tareas inactivas, la desviación es la misma. Una
  explicación posible, sin comprobar, es que los dos núcleos compiten por el
  acceso a la memoria que comparten. Las estadísticas miden tiempo, no
  vueltas, y no les afecta.
- **`3`.** *Esperado:* unos 5 s después de pulsar `b`, el aviso:
  `Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:`,
  con `- IDLE0 (CPU 0)`, y después `Tasks currently running:`, con
  `acaparadora` en el CPU 0. Se repite cada 5 s mientras `acaparadora` no se
  bloquee, y la placa no se reinicia. El teclado y el informe siguen
  respondiendo, porque van en el núcleo 1. La línea de la tecla lleva el
  instante, t, en milisegundos desde el arranque, en la misma escala que la
  marca del aviso, E (...): la diferencia es el plazo, más lo que tarde
  `acaparadora` en despertar, como mucho 100 ms. Con `i`, el tiempo de
  `acaparadora` no crece mientras gira, porque todavía no ha dejado el
  procesador, y el porcentaje de `IDLE0` baja; cuando vuelve a bloquearse, su
  tiempo da el salto.

  *Observado:* todo como se espera.
- **`4`.** *Esperado:* con `v`, el aviso nombra a `- vigilada (CPU 0)`, y la
  tarea inactiva del núcleo 0 no aparece en él. `vigilada` da su última señal
  en la vuelta anterior a la tecla, como mucho 1 s antes, así que el aviso
  sale entre 4 y 5 s después de pulsar. Entre las tareas en ejecución del CPU
  0 sale la propia `vigilada`, aunque pase casi todo el tiempo bloqueada: el
  *watchdog* vuelve a contar el plazo cuando todas las tareas vigiladas le han
  dado señal, y la última en dársela es siempre `vigilada`, al terminar el
  trabajo de cada vuelta. Cinco segundos después, que son cinco vueltas
  exactas, vuelve a estar en ese mismo punto. Las líneas de las teclas y la de
  la primera señal de `vigilada` tras volver a pulsar `v` llevan el instante,
  como en la variante 3. Si vuelves a pulsar `v` pronto, no sale ningún aviso
  más. Si lo haces en el último segundo antes de que venza el plazo, la
  siguiente señal de `vigilada` cae casi en el mismo instante en que vence,
  porque las dos van al mismo compás de un segundo, y según cuál llegue antes
  sale un aviso más o no. Con `x`, `vigilada` se bloquea en su siguiente
  vuelta, como mucho 1 s después de la tecla, y unos 5 s más tarde sale el
  aviso, que la nombra igual; en el CPU 0 se ejecuta `IDLE0`, porque
  `vigilada` está bloqueada, y en la lista sale `S`, bloqueada sin plazo en
  espera de una notificación.

  *Observado:* como se espera. Al volver a pulsar `v` pronto, no salió ningún
  aviso más; con la tecla en el último segundo se vieron los dos resultados,
  unas veces un aviso más y otra ninguno. Una sola vez, en el primer aviso
  tras un arranque, la tarea en ejecución del CPU 0 fue `IDLE0` en lugar de
  `vigilada`. El plazo vence justo cuando `vigilada` termina su trabajo, y una
  diferencia mínima en ese instante basta para que ya se haya bloqueado; es la
  explicación más probable, pero no se ha medido.
- **`5`.** *Esperado:* al entrar, el programa dice si ha podido reconfigurar
  el *watchdog*. Con `v`, pasado el plazo, el aviso termina con `Aborting.`;
  después salen la traza y `Rebooting...`, y la placa se reinicia. Al
  arrancar, el programa dice `Ultimo reinicio: watchdog de tareas` y empieza
  en la variante 1.

  *Observado:* todo como se espera.
- **`6`.** *Esperado:* el LED cambia de brillo con cada pulsación, sin saltos
  por los rebotes, y en el modo de transición baja y sube solo. Con `r` hasta
  14 bits a 5 kHz, el driver escribe
  `requested frequency 5000 and duty resolution 14 can not be achieved` y el
  programa sigue con 13 bits; a 1 kHz, los 14 bits se aceptan. A 20 kHz solo
  se aceptan 8 y 10 bits.

  *Observado:* todo como se espera: ningún modo repetido ni saltado, los
  cuatro niveles de brillo se distinguen, la transición es suave, y el driver
  acepta y rechaza justo las combinaciones calculadas.

## Detalles de las herramientas que conviene saber

- **La marca de agua va en bytes.** En ESP-IDF, la pila se mide en bytes,
  también en la columna de `vTaskList()`. En la documentación del FreeRTOS
  original, en palabras.
- **`vTaskList()` solo marca en ejecución a la tarea que la llama.** Una tarea
  que en ese momento se ejecuta en el otro núcleo sale como lista, mientras que
  `eTaskGetState()` la da en ejecución.
- **`vTaskList()` marca `S` a una tarea bloqueada sin plazo en espera de una
  notificación**, como `ipc0` e `ipc1`; `eTaskGetState()` la da bloqueada. Una
  tarea bloqueada sin plazo en una cola sí sale `B`.
- **`ipc0` e `ipc1` toman la prioridad de quien les hace un encargo.** ESP-IDF
  las usa para ejecutar una función en un núcleo concreto. Con la
  configuración de partida, cada una adopta la prioridad de la tarea que le
  hace el encargo y la conserva después (`esp_ipc.c`). En la variante 6, al
  instalar la rutina de interrupción del pulsador, la tarea `teclado` encarga
  a `ipc1` que la registre en el núcleo 1, y desde entonces `ipc1` sale con
  prioridad 9, la del teclado, en lugar de 24.
- **El reparto de `vTaskGetRunTimeStats()` es desde el arranque**, no de los
  últimos segundos. Para ver la carga de ahora hay que comparar dos lecturas,
  como hace la variante 2 con `uxTaskGetSystemState()`, que es lo que usan por
  dentro las dos tablas. El tiempo de cada tarea se suma cuando deja el
  procesador: una tarea que no lo suelta no ve crecer su tiempo hasta que lo
  hace.
- **El reloj de las estadísticas** es por omisión el de `esp_timer`, en
  microsegundos, con un contador que por omisión es de 32 bits: las cifras
  valen hasta unos 4295 s, algo más de 71 minutos, desde el arranque. La
  configuración admite un contador de 64 bits.
- **Una función de enganche de la tarea inactiva no puede bloquearse**, ni
  tardar: se ejecuta en la tarea inactiva, que tiene que poder volver a
  ejecutarse. ESP-IDF admite hasta ocho por núcleo, y el *watchdog* de tareas
  ya usa una en cada uno.

## Tabla de pines

Solo la variante 6 usa pines. Se declaran en `main/pines.h`, que llega con
`GPIO_NUM_NC`, «sin conectar»: los eliges tú, como en U2-S1.

| Señal | Constante | Montaje |
| --- | --- | --- |
| LED | `PIN_LED` | LED y resistencia entre el pin y masa (`LED_NIVEL_ENCENDIDO` 1) o entre el pin y 3,3 V (0) |
| Pulsador | `PIN_PULSADOR` | Entre el pin y masa, con el *pull-up* interno (`PULSADOR_NIVEL_PULSADO` 0), como en U2-S2 |

## Qué puedes cambiar sin tocar el código

En el menú de configuración, **SEyTR - Depuracion de tareas** tiene tres
opciones: permitir la variante 5 y, para el antirrebote, el tiempo de guarda,
30 ms por omisión, y la espera antes de confirmar el nivel, 20 ms. El plazo del *watchdog* de tareas y si llama al gestor de pánico
están en **Component config → ESP System Settings**. El proyecto fija en
`sdkconfig.defaults` el destino `esp32s3`, el tick a 1000 Hz y las cuatro
opciones de inspección. Si en tu `sdkconfig` hay otro valor fijado a mano,
manda sobre `sdkconfig.defaults`: cámbialo en la configuración o borra
`sdkconfig` y vuelve a fijar el destino.

## Estructura

```
CMakeLists.txt              define el proyecto
sdkconfig.defaults          configuración de partida; se escribe a mano y se versiona
main/
  CMakeLists.txt            registra las fuentes del componente principal
  Kconfig.projbuild         las opciones propias del ejemplo
  pines.h                   los pines del LED y del pulsador
  depuracion_de_tareas.c    app_main, la elección de variante, el teclado y el informe
  inspeccion.c              la variante 1
  carga.c                   la variante 2
  vigilancia.c              las variantes 3, 4 y 5
  salida_modulada.c         la variante 6
  comun.c, comun.h          el trabajo simulado, la tabla de tareas y la memoria
components/
  pulsador/                 el componente del pulsador de U2-S2, copiado sin cambios;
                            su README nombra el componente led de aquel ejemplo
```

No se versionan el fichero `sdkconfig`, que genera la herramienta a partir de
`sdkconfig.defaults`, ni la carpeta `build/`.

Para cambiar de variante, el programa guarda la elegida en una variable
marcada con `__NOINIT_ATTR`, que conserva su valor tras un reinicio por
programa, y reinicia la placa. Tras un reinicio que no es por programa, como
el del gestor de pánico, arranca en la variante 1.

## Fuentes

- Espressif, guía de ESP-IDF v6.1 para el ESP32-S3,
  [vigilantes (watchdogs)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/wdts.html):
  el *watchdog* de tareas, su comportamiento por omisión, la suscripción de una
  tarea y la reconfiguración; el *watchdog* de interrupciones.
- Espressif, guía de ESP-IDF v6.1,
  [FreeRTOS (Supplemental Features)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/freertos_additions.html),
  apartado «ESP-IDF Tick and Idle Hooks»: las funciones de enganche de la
  tarea inactiva y lo que significa lo que devuelven.
- Espressif, guía de ESP-IDF v6.1,
  [control de LED (LEDC)](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/peripherals/ledc.html):
  modo de baja velocidad, relación entre frecuencia y resolución, funciones
  seguras entre tareas, transiciones y su aviso de fin.
- Espressif, guía de ESP-IDF v6.1,
  [asignación de memoria](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32s3/api-reference/system/mem_alloc.html):
  `heap_caps_get_free_size()` y las demás funciones de memoria libre.
- Espressif, ESP-IDF v6.1,
  [`Kconfig` de FreeRTOS](https://github.com/espressif/esp-idf/blob/v6.1/components/freertos/Kconfig):
  las opciones de inspección y el reloj de las estadísticas.
- Espressif, ESP-IDF v6.1,
  [`Kconfig` de `esp_system`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_system/Kconfig):
  el plazo del *watchdog* de tareas y las tareas inactivas que vigila.
- Espressif, ESP-IDF v6.1,
  [`task_wdt.c`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_system/task_wdt/task_wdt.c):
  el texto del aviso.
- Espressif, ESP-IDF v6.1,
  [`ledc.c`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_driver_ledc/src/ledc.c):
  el cálculo del divisor del temporizador y el error de las combinaciones que
  no puede dar.
- R. Barry, [*Mastering the FreeRTOS Real Time Kernel*](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book),
  capítulo 4, apartado 4.8, la tarea inactiva y su función de enganche, y
  capítulo 13, apartado 13.3, el desbordamiento de pila y
  `uxTaskGetStackHighWaterMark()`.

## Un aviso

Este repositorio es material de la asignatura. El repositorio de tu **proyecto
final** es otro distinto, con su propio nombre y sus propias reglas.
