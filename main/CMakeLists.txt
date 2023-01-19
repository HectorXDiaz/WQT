set(component_srcs "ssd1306.c" "ssd1306_i2c.c" "ssd1306_spi.c")

idf_component_register(SRCS "codigo2.c" "main.c"
                    INCLUDE_DIRS ".")
