idf_component_register(SRCS "main.c"
                            "gfx.c"
                            "font5x7.c"
                            "touch.c"
                       INCLUDE_DIRS "."
                       # The project is built with MINIMAL_BUILD, so every
                       # component used here has to be listed explicitly.
                       REQUIRES esp_lcd
                                esp_driver_spi
                                esp_driver_gpio
                                esp_driver_ledc
                                esp_timer
                                espressif__esp_lcd_ili9341
                                atanisoft__esp_lcd_touch_xpt2046)
