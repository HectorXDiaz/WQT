# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
idf_component_register(SRCS "app_main.c" "ds18b20.c" "telegram.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES telegram_certificate.pem)