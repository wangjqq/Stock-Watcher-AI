idf_component_register(SRCS "main.c"
                            "hardware/led/bsp_led.c"
                            "hardware/key/bsp_key.c"
                    INCLUDE_DIRS "."
                                 "hardware/led"
                                 "hardware/key"
                      )