idf_component_register(SRCS "ds18b20.c" "dallas.c" 
                    PRIV_REQUIRES driver
                    REQUIRES esp_timer 
                    INCLUDE_DIRS "."
                    "include")
