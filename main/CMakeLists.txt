# CMakeLists.txt de componente. Registra las fuentes del programa.
# Si anades un fichero .c, anadelo tambien a SRCS.
# PRIV_REQUIRES dice de que componentes depende: esp_timer, el reloj en
# microsegundos; esp_system, el reinicio, el watchdog de tareas y las funciones
# de enganche de la tarea inactiva; esp_driver_ledc, la salida modulada;
# esp_driver_gpio, la interrupcion del pulsador; y pulsador, el componente del
# propio proyecto.
idf_component_register(SRCS "depuracion_de_tareas.c" "comun.c" "inspeccion.c"
                            "carga.c" "vigilancia.c" "salida_modulada.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_timer esp_system esp_driver_ledc
                                     esp_driver_gpio pulsador)
